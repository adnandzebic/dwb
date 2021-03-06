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

#ifndef UTIL_H
#define UTIL_H


// strings
char * util_string_replace(const char *haystack, const char *needle, const char *replacemant);
void util_cut_text(char *, int, int);

gboolean util_is_hex(const char *string);
int util_test_connect(const char *uri);

char * util_get_temp_filename(const char *);
void util_rmdir(const char *path, gboolean, gboolean recursive);

// keys
char * dwb_modmask_to_string(guint );
char * util_keyval_to_char(guint, gboolean);

// arg
char * util_arg_to_char(Arg *, DwbType );
Arg * util_char_to_arg(char *, DwbType );

// sort 
int util_navigation_compare_first(Navigation *, Navigation *);
int util_navigation_compare_second(Navigation *, Navigation *);
int util_navigation_compare_uri(Navigation *, const char *);

int util_keymap_sort_first(KeyMap *, KeyMap *);
int util_keymap_sort_second(KeyMap *, KeyMap *);
int util_web_settings_sort_second(WebSettings *, WebSettings *);
int util_web_settings_sort_first(WebSettings *, WebSettings *);

// files
char * util_expand_home(char *buffer, const char *filename, size_t length);
char * util_normalize_filename(char *buffer, const char *filename, size_t length);
void util_get_directory_content(GString *, const char *, const char *extension);
char * util_get_file_content(const char *, gsize *);
char ** util_get_lines(const char *);
gboolean util_set_file_content(const char *, const char *);
char * util_build_path(void);
char * util_get_system_data_dir(const char *);
char * util_get_user_data_dir(const char *);
char * util_get_data_file(const char *, const char *);
char * util_get_data_dir(const char *dir);

// navigation
Navigation * dwb_navigation_new_from_line(const char *);
Navigation * dwb_navigation_new(const char *, const char *);
Navigation * dwb_navigation_dup(Navigation *n);
void dwb_navigation_free(Navigation *);

void dwb_web_settings_free(WebSettings *);

// quickmarks
void dwb_quickmark_free(Quickmark *);
Quickmark * dwb_quickmark_new(const char *, const char *, const char *);
Quickmark * dwb_quickmark_new_from_line(const char *);
int util_quickmark_compare(Quickmark *a, Quickmark *b);
int util_quickmark_compare_uri(Quickmark *a, const char *);

// useless
char * dwb_return(const char *);

void * dwb_malloc(size_t);

char * util_domain_from_uri(const char *);
int util_compare_path(const char *, const char *);
int util_compare_first_word(const char *, const char *);
char * util_basename(const char *);

gboolean util_file_add(const char *filename, const char *text, int, int);
gboolean util_file_add_navigation(const char *, const Navigation *, int, int);

void gtk_box_insert(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, gint padding, int position, GtkPackType);
void gtk_widget_remove_from_parent(GtkWidget *);

char * util_strcasestr(const char *haystack, const char *needle);
int util_file_remove_line(const char *, const char *);
Arg * util_arg_new(void);
char * util_check_directory(char *);
int util_strlen_trailing_space(const char *str);
const char * util_str_chug(const char *str);
Sanitize util_string_to_sanitize(const char *);

char *util_create_json(int, ...);
char *util_create_json(int, ...);


#endif
