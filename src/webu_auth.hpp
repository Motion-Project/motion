/*
 *    webu_auth.hpp
 *
 *    Password hashing and authentication utilities for Motion web server
 *    Uses libxcrypt for bcrypt password hashing (work factor 12)
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

#ifndef _INCLUDE_WEBU_AUTH_HPP_
#define _INCLUDE_WEBU_AUTH_HPP_

#include <string>

/**
 * Password hashing and verification using bcrypt via libxcrypt
 */
class cls_webu_auth {
public:
    /**
     * Hash password with bcrypt (work factor 12)
     *
     * @param password Plaintext password to hash
     * @return Bcrypt hash string ($2b$12$...) or empty string on error
     *
     * Thread-safe: Uses crypt_r() for thread safety
     * Performance: ~150ms on Raspberry Pi 4 with work factor 12
     */
    static std::string hash_password(const std::string &password);

    /**
     * Verify password against bcrypt hash
     *
     * @param password Plaintext password to verify
     * @param hash Bcrypt hash to verify against
     * @return true if password matches hash, false otherwise
     *
     * Thread-safe: Uses crypt_r() for thread safety
     * Performance: ~150ms on Raspberry Pi 4 with work factor 12
     */
    static bool verify_password(const std::string &password,
                                const std::string &hash);

    /**
     * Check if string is a bcrypt hash
     *
     * @param str String to check
     * @return true if starts with $2b$ or $2a$ (bcrypt format)
     *
     * Bcrypt hashes are exactly 60 characters and start with $2b$ or $2a$
     */
    static bool is_bcrypt_hash(const std::string &str);

    /**
     * Generate cryptographically secure random password
     *
     * @return 16-character random password (alphanumeric + symbols)
     *
     * Uses /dev/urandom for cryptographic quality randomness
     * Charset: a-zA-Z0-9 plus !@#$%^&*
     */
    static std::string generate_random_password();

private:
    /**
     * Generate bcrypt salt with work factor 12
     *
     * @return Bcrypt salt string ($2b$12$...) or empty string on error
     *
     * Internal use only - called by hash_password()
     */
    static std::string generate_salt();
};

#endif /* _INCLUDE_WEBU_AUTH_HPP_ */
