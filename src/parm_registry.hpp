/*
 *    This file is part of Motion.
 *
 *    Motion is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Motion is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Motion.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/*
 * parm_registry.hpp - Parameter Registry for O(1) Lookups
 *
 * This module provides a centralized registry for configuration parameters
 * that enables O(1) lookup by name via hash map, replacing the previous
 * O(n) linear array iteration pattern.
 *
 * Part of the configuration system refactoring for Pi 5 performance optimization.
 * See doc/plans/ConfigParam-Refactor-20251211-1730.md for full design.
 */

#ifndef _INCLUDE_PARM_REGISTRY_HPP_
#define _INCLUDE_PARM_REGISTRY_HPP_

#include <string>
#include <vector>
#include <unordered_map>
#include "conf.hpp"

/* Parameter scope flags - which devices use this parameter */
enum PARM_SCOPE {
    PARM_SCOPE_APP    = 0x01,   /* Application-level only (daemon, webcontrol, database) */
    PARM_SCOPE_CAM    = 0x02,   /* Camera devices (detection, capture, output) */
    PARM_SCOPE_SND    = 0x04,   /* Sound devices (sound alerts) */
    PARM_SCOPE_ALL    = 0x07    /* All scopes */
};

/*
 * Extended parameter definition for registry
 * Adds scope information to the existing ctx_parm struct
 */
struct ctx_parm_ext {
    std::string     parm_name;      /* Parameter name */
    enum PARM_TYP   parm_type;      /* Type: STRING, INT, LIST, BOOL, ARRAY, PARAMS */
    enum PARM_CAT   parm_cat;       /* Category for web UI grouping */
    int             webui_level;    /* Web UI display level */
    int             scope;          /* PARM_SCOPE flags for device filtering */
};

/*
 * Singleton registry for O(1) parameter lookups
 *
 * Provides:
 * - O(1) lookup by parameter name via hash map
 * - Iteration by category (for web UI display)
 * - Iteration by scope (for device initialization)
 * - Full list access (for serialization)
 * - Hot reload status check for runtime updates
 *
 * Usage:
 *   const ctx_parm_ext *p = ctx_parm_registry::instance().find("threshold");
 *   if (p) {
 *       // Use parameter definition
 *   }
 */

/*
 * Check if a parameter can be hot-reloaded without restart
 * Returns true if the parameter exists and is hot-reloadable
 * Returns false if the parameter doesn't exist or requires restart
 */
bool is_hot_reloadable(const std::string &parm_name);

/*
 * Get parameter info from the config_parms array
 * Returns pointer to ctx_parm entry, or nullptr if not found
 */
const ctx_parm* get_parm_info(const std::string &parm_name);
class ctx_parm_registry {
public:
    /*
     * Get singleton instance
     * Thread-safe via C++11 static initialization guarantee
     */
    static ctx_parm_registry& instance();

    /*
     * O(1) lookup by parameter name
     * Returns nullptr if not found
     */
    const ctx_parm_ext *find(const std::string &name) const;

    /*
     * Get parameters by category (for web UI)
     * Returns empty vector if category out of range
     */
    const std::vector<const ctx_parm_ext*>& by_category(enum PARM_CAT cat) const;

    /*
     * Get parameters by scope (for device initialization)
     * scope: PARM_SCOPE flags (can be combined with bitwise OR)
     */
    std::vector<const ctx_parm_ext*> by_scope(int scope) const;

    /*
     * Get full parameter list (for serialization/iteration)
     */
    const std::vector<ctx_parm_ext>& all() const { return parm_vec; }

    /*
     * Get parameter count
     */
    size_t size() const { return parm_vec.size(); }

    /* Prevent copying */
    ctx_parm_registry(const ctx_parm_registry&) = delete;
    ctx_parm_registry& operator=(const ctx_parm_registry&) = delete;

private:
    /* Private constructor - initializes from config_parms[] */
    ctx_parm_registry();

    /* Master list of all parameters with extended info */
    std::vector<ctx_parm_ext> parm_vec;

    /* Hash map: parameter name -> index in parm_vec */
    std::unordered_map<std::string, size_t> parm_map;

    /* Parameters indexed by category for fast category lookup */
    std::vector<std::vector<const ctx_parm_ext*>> by_cat;

    /* Empty vector for out-of-range category requests */
    static const std::vector<const ctx_parm_ext*> empty_vec;
};

#endif /* _INCLUDE_PARM_REGISTRY_HPP_ */
