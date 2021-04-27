/*
    Copyright (C) 2024  John Cox john.cox@raspberrypi.com

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

#ifndef AVCODEC_WEAK_LINK_H
#define AVCODEC_WEAK_LINK_H

struct ff_weak_link_master;
struct ff_weak_link_client;

struct ff_weak_link_master * ff_weak_link_new(void * p);
void ff_weak_link_break(struct ff_weak_link_master ** ppLink);

struct ff_weak_link_client* ff_weak_link_ref(struct ff_weak_link_master * w);
void ff_weak_link_unref(struct ff_weak_link_client ** ppLink);

// Returns NULL if link broken - in this case it will also zap
//   *ppLink and unref the weak_link.
// Returns NULL if *ppLink is NULL (so a link once broken stays broken)
//
// The above does mean that there is a race if this is called simultainiously
// by two threads using the same weak_link_client (so don't do that)
void * ff_weak_link_lock(struct ff_weak_link_client ** ppLink);
void ff_weak_link_unlock(struct ff_weak_link_client * c);

#endif
