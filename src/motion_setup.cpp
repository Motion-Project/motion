/*
 *    motion_setup.cpp
 *
 *    CLI tool for Motion authentication setup
 *    Prompts for admin and viewer passwords, hashes with bcrypt, saves to config
 *
 *    Copyright 2026 Motion Project
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 */

#include "webu_auth.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <random>
#include <sys/stat.h>

const char* DEFAULT_CONFIG_PATH = "/usr/local/etc/motion/motion.conf";
const char* ALT_CONFIG_PATH = "/etc/motion/motion.conf";
const char* INITIAL_PASSWORD_FILE = "/var/lib/motion/initial-password.txt";
const int MAX_PASSWORD_ATTEMPTS = 3;
const int GENERATED_PASSWORD_LENGTH = 16;

/**
 * Find the best config file path
 * Checks common locations and returns the first one that exists,
 * or the default path if none exist (will be created)
 */
std::string find_config_path() {
    /* Check if alternate path exists (package install location) */
    std::ifstream alt_test(ALT_CONFIG_PATH);
    if (alt_test) {
        alt_test.close();
        return ALT_CONFIG_PATH;
    }

    /* Check if default path exists (source install location) */
    std::ifstream def_test(DEFAULT_CONFIG_PATH);
    if (def_test) {
        def_test.close();
        return DEFAULT_CONFIG_PATH;
    }

    /* Neither exists - return default (will be created) */
    return DEFAULT_CONFIG_PATH;
}

/**
 * Generate a cryptographically secure random password
 */
std::string generate_random_password() {
    const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*";

    std::string password;
    password.reserve(GENERATED_PASSWORD_LENGTH);

    /* Use /dev/urandom for crypto-quality randomness */
    FILE *fp = fopen("/dev/urandom", "rb");
    if (!fp) {
        /* Fallback to less secure random if urandom unavailable */
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
        for (int i = 0; i < GENERATED_PASSWORD_LENGTH; i++) {
            password += charset[dis(gen)];
        }
        return password;
    }

    unsigned char random_bytes[GENERATED_PASSWORD_LENGTH];
    size_t bytes_read = fread(random_bytes, 1, GENERATED_PASSWORD_LENGTH, fp);
    fclose(fp);

    if (bytes_read != GENERATED_PASSWORD_LENGTH) {
        return "";  /* Error reading random bytes */
    }

    for (int i = 0; i < GENERATED_PASSWORD_LENGTH; i++) {
        password += charset[random_bytes[i] % (sizeof(charset) - 1)];
    }

    return password;
}

/**
 * Save generated password to a temporary file for recovery
 * File is readable only by root
 */
bool save_initial_password_file(const std::string &admin_pass, const std::string &viewer_user,
                                 const std::string &viewer_pass, bool admin_generated, bool viewer_generated) {
    /* Create directory if it doesn't exist */
    mkdir("/var/lib/motion", 0755);

    FILE *fp = fopen(INITIAL_PASSWORD_FILE, "w");
    if (!fp) {
        return false;
    }

    fprintf(fp, "Motion Initial Password Recovery\n");
    fprintf(fp, "=================================\n\n");
    fprintf(fp, "This file contains auto-generated passwords.\n");
    fprintf(fp, "DELETE THIS FILE after saving the passwords securely.\n\n");

    if (admin_generated) {
        fprintf(fp, "Admin username:  admin\n");
        fprintf(fp, "Admin password:  %s  (AUTO-GENERATED)\n\n", admin_pass.c_str());
    }

    if (viewer_generated) {
        fprintf(fp, "Viewer username: %s\n", viewer_user.c_str());
        fprintf(fp, "Viewer password: %s  (AUTO-GENERATED)\n\n", viewer_pass.c_str());
    }

    fprintf(fp, "To change passwords later, run: sudo motion-setup --reset\n");
    fclose(fp);

    /* Set restrictive permissions (root read-only) */
    chmod(INITIAL_PASSWORD_FILE, 0600);

    return true;
}

/**
 * Get password from user without echoing to terminal
 */
std::string get_password(const char *prompt) {
    struct termios old_term, new_term;

    /* Turn off echo */
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    std::cout << prompt << ": " << std::flush;
    std::string password;
    std::getline(std::cin, password);
    std::cout << std::endl;

    /* Restore echo */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

    return password;
}

/**
 * Ensure config directory and file exist
 * Creates directory and minimal config file if needed
 */
bool ensure_config_exists(const std::string &config_path) {
    /* Extract directory from config path */
    size_t last_slash = config_path.rfind('/');
    if (last_slash != std::string::npos) {
        std::string dir_path = config_path.substr(0, last_slash);

        /* Create directory if it doesn't exist (with parents) */
        std::string mkdir_cmd = "mkdir -p " + dir_path;
        if (system(mkdir_cmd.c_str()) != 0) {
            std::cerr << "Warning: Could not create directory: " << dir_path << std::endl;
        }
    }

    /* Check if config file exists */
    std::ifstream test(config_path);
    if (!test) {
        /* Create minimal config file */
        std::ofstream outfile(config_path);
        if (!outfile) {
            std::cerr << "Error: Cannot create config file: " << config_path << std::endl;
            return false;
        }

        outfile << "# Motion configuration file\n";
        outfile << "# Created by motion-setup\n";
        outfile << "#\n";
        outfile << "# See motion-dist.conf for all available options\n";
        outfile << "\n";
        outfile << "# Web control interface\n";
        outfile << "webcontrol_port 8080\n";
        outfile << "webcontrol_localhost off\n";
        outfile << "webcontrol_parms 2\n";
        outfile << "\n";
        outfile << "# Authentication (configured by motion-setup)\n";
        outfile.close();

        std::cout << "Created new config file: " << config_path << "\n";
    }

    return true;
}

/**
 * Update a parameter in the config file
 * Returns true on success, false on failure
 */
bool update_config_parameter(const std::string &config_path,
                             const std::string &param_name,
                             const std::string &param_value) {
    /* Ensure config file exists first */
    if (!ensure_config_exists(config_path)) {
        return false;
    }

    /* Read entire file */
    std::ifstream infile(config_path);
    if (!infile) {
        std::cerr << "Error: Cannot read config file: " << config_path << std::endl;
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    bool found = false;

    while (std::getline(infile, line)) {
        /* Check if this line sets the parameter (handle comments and whitespace) */
        std::string trimmed = line;
        /* Remove leading whitespace */
        size_t start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) {
            trimmed = trimmed.substr(start);
        }

        /* Skip commented lines */
        if (!trimmed.empty() && trimmed[0] != ';' && trimmed[0] != '#') {
            /* Check if line starts with parameter name */
            if (trimmed.find(param_name) == 0) {
                /* Check if followed by whitespace or end of string */
                size_t param_len = param_name.length();
                if (trimmed.length() == param_len ||
                    trimmed[param_len] == ' ' ||
                    trimmed[param_len] == '\t') {
                    /* Replace this line */
                    lines.push_back(param_name + " " + param_value);
                    found = true;
                    continue;
                }
            }
        }

        /* Keep original line */
        lines.push_back(line);
    }
    infile.close();

    /* If parameter not found, append it */
    if (!found) {
        lines.push_back(param_name + " " + param_value);
    }

    /* Write back to file */
    std::ofstream outfile(config_path);
    if (!outfile) {
        std::cerr << "Error: Cannot write config file: " << config_path << std::endl;
        return false;
    }

    for (const auto &l : lines) {
        outfile << l << "\n";
    }

    return true;
}

int main(int argc, char **argv) {
    bool reset_mode = false;
    bool config_specified = false;
    std::string config_path;

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--reset") == 0) {
            reset_mode = true;
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[i + 1];
            config_specified = true;
            i++;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            std::cout << "Motion Authentication Setup\n";
            std::cout << "============================\n\n";
            std::cout << "Usage: motion-setup [OPTIONS]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --reset              Reset forgotten passwords\n";
            std::cout << "  --config PATH        Use alternate config file (auto-detected if not specified)\n";
            std::cout << "  --help, -h           Show this help message\n\n";
            std::cout << "Config file locations checked:\n";
            std::cout << "  1. /etc/motion/motion.conf (package install)\n";
            std::cout << "  2. /usr/local/etc/motion/motion.conf (source install)\n\n";
            std::cout << "This tool configures Motion authentication by:\n";
            std::cout << "  1. Prompting for admin password (username: admin)\n";
            std::cout << "  2. Prompting for viewer username and password\n";
            std::cout << "  3. Hashing passwords with bcrypt (work factor 12)\n";
            std::cout << "  4. Updating config file\n\n";
            std::cout << "Note: Must be run as root to write to config file\n";
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            std::cerr << "Use --help for usage information\n";
            return 1;
        }
    }

    /* Check root privileges */
    if (geteuid() != 0) {
        std::cerr << "Error: motion-setup must be run as root\n";
        std::cerr << "Use: sudo motion-setup\n";
        return 1;
    }

    /* Auto-detect config path if not specified */
    if (!config_specified) {
        config_path = find_config_path();
    }

    std::cout << "Motion Authentication Setup\n";
    std::cout << "============================\n\n";

    std::cout << "Config file: " << config_path << "\n\n";

    if (reset_mode) {
        std::cout << "Password Reset Mode\n\n";
    } else {
        std::cout << "This wizard will configure authentication for Motion.\n";
        std::cout << "You'll create two accounts:\n";
        std::cout << "  - admin: Full access (view, configure, control)\n";
        std::cout << "  - viewer: Read-only access (view only)\n\n";
    }

    /* Admin password with retry logic */
    std::cout << "Admin Account (username: admin)\n";
    std::cout << "--------------------------------\n";

    std::string admin_pass;
    bool admin_generated = false;
    int admin_attempts = 0;

    while (admin_attempts < MAX_PASSWORD_ATTEMPTS) {
        admin_pass = get_password("Admin password");
        std::string admin_pass_confirm = get_password("Confirm password");

        if (admin_pass.empty()) {
            admin_attempts++;
            std::cerr << "Error: Password cannot be empty. "
                      << (MAX_PASSWORD_ATTEMPTS - admin_attempts) << " attempts remaining.\n\n";
            continue;
        }

        if (admin_pass != admin_pass_confirm) {
            admin_attempts++;
            std::cerr << "Error: Passwords don't match. "
                      << (MAX_PASSWORD_ATTEMPTS - admin_attempts) << " attempts remaining.\n\n";
            continue;
        }

        break;  /* Success */
    }

    /* If all attempts failed, generate random password */
    if (admin_attempts >= MAX_PASSWORD_ATTEMPTS) {
        admin_pass = generate_random_password();
        admin_generated = true;

        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ⚠  ADMIN PASSWORD SET TO AUTO-GENERATED VALUE              ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
        std::cout << "║                                                              ║\n";
        std::cout << "║  Admin password: " << admin_pass;
        /* Pad to align box */
        for (size_t i = admin_pass.length(); i < 40; i++) std::cout << " ";
        std::cout << "║\n";
        std::cout << "║                                                              ║\n";
        std::cout << "║  SAVE THIS PASSWORD - It will not be shown again!            ║\n";
        std::cout << "║  Change later with: sudo motion-setup --reset                ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }

    /* Viewer credentials with retry logic */
    std::cout << "\nViewer Account\n";
    std::cout << "---------------\n";
    std::cout << "Username [viewer]: " << std::flush;

    std::string viewer_user;
    std::getline(std::cin, viewer_user);
    if (viewer_user.empty()) {
        viewer_user = "viewer";
    }

    std::string viewer_pass;
    bool viewer_generated = false;
    int viewer_attempts = 0;

    while (viewer_attempts < MAX_PASSWORD_ATTEMPTS) {
        viewer_pass = get_password("Viewer password");
        std::string viewer_pass_confirm = get_password("Confirm password");

        if (viewer_pass.empty()) {
            viewer_attempts++;
            std::cerr << "Error: Password cannot be empty. "
                      << (MAX_PASSWORD_ATTEMPTS - viewer_attempts) << " attempts remaining.\n\n";
            continue;
        }

        if (viewer_pass != viewer_pass_confirm) {
            viewer_attempts++;
            std::cerr << "Error: Passwords don't match. "
                      << (MAX_PASSWORD_ATTEMPTS - viewer_attempts) << " attempts remaining.\n\n";
            continue;
        }

        break;  /* Success */
    }

    /* If all attempts failed, generate random password */
    if (viewer_attempts >= MAX_PASSWORD_ATTEMPTS) {
        viewer_pass = generate_random_password();
        viewer_generated = true;

        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ⚠  VIEWER PASSWORD SET TO AUTO-GENERATED VALUE             ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
        std::cout << "║                                                              ║\n";
        std::cout << "║  Viewer password: " << viewer_pass;
        /* Pad to align box */
        for (size_t i = viewer_pass.length(); i < 39; i++) std::cout << " ";
        std::cout << "║\n";
        std::cout << "║                                                              ║\n";
        std::cout << "║  SAVE THIS PASSWORD - It will not be shown again!            ║\n";
        std::cout << "║  Change later with: sudo motion-setup --reset                ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }

    /* Hash passwords */
    std::cout << "\nHashing passwords with bcrypt (this may take a few seconds)...\n";

    std::string admin_hash = cls_webu_auth::hash_password(admin_pass);
    if (admin_hash.empty()) {
        std::cerr << "Error: Failed to hash admin password\n";
        return 1;
    }

    std::string viewer_hash = cls_webu_auth::hash_password(viewer_pass);
    if (viewer_hash.empty()) {
        std::cerr << "Error: Failed to hash viewer password\n";
        return 1;
    }

    /* Update config file */
    std::cout << "Updating config file: " << config_path << "\n";

    std::string admin_value = "admin:" + admin_hash;
    if (!update_config_parameter(config_path, "webcontrol_authentication", admin_value)) {
        return 1;
    }

    std::string viewer_value = viewer_user + ":" + viewer_hash;
    if (!update_config_parameter(config_path, "webcontrol_user_authentication", viewer_value)) {
        return 1;
    }

    /* Save initial password file if any passwords were auto-generated */
    if (admin_generated || viewer_generated) {
        if (save_initial_password_file(admin_pass, viewer_user, viewer_pass,
                                        admin_generated, viewer_generated)) {
            std::cout << "Auto-generated passwords saved to: " << INITIAL_PASSWORD_FILE << "\n";
            std::cout << "(Delete this file after saving passwords securely)\n\n";
        }
    }

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          Configuration Updated Successfully                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Admin username:  admin\n";
    if (admin_generated) {
        std::cout << "Admin password:  " << admin_pass << "  ⚠ AUTO-GENERATED\n\n";
    } else {
        std::cout << "Admin password:  (as entered)\n\n";
    }

    std::cout << "Viewer username: " << viewer_user << "\n";
    if (viewer_generated) {
        std::cout << "Viewer password: " << viewer_pass << "  ⚠ AUTO-GENERATED\n\n";
    } else {
        std::cout << "Viewer password: (as entered)\n\n";
    }

    std::cout << "Passwords have been hashed with bcrypt (work factor 12)\n";
    std::cout << "Config file updated: " << config_path << "\n\n";

    if (admin_generated || viewer_generated) {
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ⚠  IMPORTANT: Save auto-generated passwords NOW!           ║\n";
        std::cout << "║     They are also saved to: " << INITIAL_PASSWORD_FILE << "    ║\n";
        std::cout << "║     Delete that file after saving passwords securely.        ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
    }

    std::cout << "Restart Motion to apply changes:\n";
    std::cout << "  sudo systemctl restart motion\n\n";

    return 0;
}
