/*
 * Bael'Zharon's Respite
 * Copyright (C) 2014 Daniel Skorupski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef BZR_CONFIG_H
#define BZR_CONFIG_H

#include "Noncopyable.h"

struct json_t;

class Config : Noncopyable
{
public:
    Config();
    ~Config();

    void setBool(const char* name, bool value);
    void setInt(const char* name, int value);
    void setFloat(const char* name, fp_t value);
    void setString(const char* name, const string& value);

    bool getBool(const char* name, bool defaultValue);
    int getInt(const char* name, int defaultValue);
    fp_t getFloat(const char* name, fp_t defaultValue);
    string getString(const char* name, const string& defaultValue);

    void erase(const char* name);

private:
    void set(const char* name, json_t* value);
    json_t* get(const char* name) const;

    string path_;
    json_t* root_;
};

#endif