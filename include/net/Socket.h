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
#ifndef BZR_NET_SOCKET_H
#define BZR_NET_SOCKET_H

#include "Noncopyable.h"
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <array>
#include <chrono>

const size_t kDatagramMaxSize = 512;

#ifdef _WIN32
typedef SOCKET SocketType;
#else
typedef int SocketType;
#endif

struct Packet
{
    uint32_t remoteIp;
    uint16_t remotePort;
    size_t size;
    array<uint8_t, kDatagramMaxSize> data;
};

class Socket : Noncopyable
{
public:
    Socket();
    ~Socket();

    bool wait(chrono::microseconds timeout);
    void read(Packet& packet);
    void write(const Packet& packet);

private:
    SocketType sock_;
};

#endif