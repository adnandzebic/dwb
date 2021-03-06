/*
 * Copyright (c) 2010-2012 Stefan Bolte <portix@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef COOKIES_H
#define COOKIES_H

void dwb_soup_clean(void);
void dwb_soup_sync_cookies(void);
void dwb_soup_allow_cookie_tmp(void);
DwbStatus dwb_soup_allow_cookie(GList **, const char *, CookieStorePolicy);
const char * dwb_soup_get_host_from_request(WebKitNetworkRequest *);
SoupMessage * dwb_soup_get_message(WebKitWebFrame *);
SoupMessage * dwb_soup_get_message_from_wv(WebKitWebView *);
DwbStatus dwb_soup_set_cookie_accept_policy(const char *);
void dwb_soup_set_ntlm(gboolean);
void dwb_soup_cookies_set_accept_policy(Arg *);
void dwb_soup_save_cookies(GSList *);
void dwb_soup_init_cookies(SoupSession *);
void dwb_soup_init_proxy();
DwbStatus dwb_soup_init_session_features();
void dwb_soup_end();
void dwb_soup_init();
void dwb_soup_clear_cookies();
const char * soup_get_header(GList *gl, const char *);
const char * soup_get_header_from_request(WebKitNetworkRequest *, const char *);
CookieStorePolicy dwb_soup_get_cookie_store_policy(const char *policy);
void dwb_soup_clear_cookies(void);

#endif
