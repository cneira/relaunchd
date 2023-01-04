/*
 * Copyright (c) 2022 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdexcept>
#include <string>
#include <unistd.h>

#include "config.h"
#include "options.h"

std::string getConfigDir() {
    std::string configdir;
    if (getuid() == 0) {
        return "/etc/relaunchd"; // TODO: add to config.h
    } else {
        // The subdirectory under $HOME where configuration for the user domains
        // are stored Can be overridden by setting the $XDG_CONFIG_HOME variable
        const char *xdg_config_home = getenv("XDG_CONFIG_HOME");
        if (xdg_config_home) {
            configdir = std::string{xdg_config_home};
        } else {
            auto home = getenv("HOME");
            if (home) {
                configdir = std::string{home} + "/.local/config";
            } else {
                throw std::runtime_error("No HOME environment variable is set");
            }
        }
        configdir += "/relaunchd";
    }
    return configdir;
}
