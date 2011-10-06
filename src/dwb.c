/*
 * Copyright (c) 2010-2011 Stefan Bolte <portix@gmx.net>
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

#include "dwb.h"
#include "soup.h"
#include "completion.h"
#include "commands.h"
#include "view.h"
#include "util.h"
#include "download.h"
#include "session.h"
#include "config.h"
#include "icon.xpm"
#include "html.h"
#include "plugins.h"


/* DECLARATIONS {{{*/
static void dwb_webkit_setting(GList *, WebSettings *);
static void dwb_webview_property(GList *, WebSettings *);
void dwb_set_background_tab(GList *, WebSettings *);
static void dwb_set_scripts(GList *, WebSettings *);
static void dwb_set_user_agent(GList *, WebSettings *);
static void dwb_set_dummy(GList *, WebSettings *);
static void dwb_set_startpage(GList *, WebSettings *);
static void dwb_set_message_delay(GList *, WebSettings *);
static void dwb_set_history_length(GList *, WebSettings *);
static void dwb_set_plugin_blocker(GList *, WebSettings *);
static void dwb_set_adblock(GList *, WebSettings *);
static void dwb_set_hide_tabbar(GList *, WebSettings *);
static void dwb_set_sync_interval(GList *, WebSettings *);
static void dwb_set_private_browsing(GList *, WebSettings *);
static void dwb_reload_scripts(GList *, WebSettings *);
static void dwb_follow_selection(void);
static Navigation * dwb_get_search_completion_from_navigation(Navigation *);
static gboolean dwb_sync_history(gpointer);


static TabBarVisible dwb_eval_tabbar_visible(const char *arg);

static void dwb_reload_layout(GList *,  WebSettings *);
static char * dwb_test_userscript(const char *);

static void dwb_open_channel(const char *);
static void dwb_open_si_channel(void);
static gboolean dwb_handle_channel(GIOChannel *c, GIOCondition condition, void *data);

static gboolean dwb_eval_key(GdkEventKey *);


static void dwb_tab_label_set_text(GList *, const char *);

static void dwb_save_quickmark(const char *);
static void dwb_open_quickmark(const char *);


static void dwb_init_key_map(void);
static void dwb_init_settings(void);
static void dwb_init_style(void);
static void dwb_init_gui(void);
static void dwb_init_vars(void);

static Navigation * dwb_get_search_completion(const char *text);

static void dwb_clean_vars(void);
/*}}}*/

static int signals[] = { SIGFPE, SIGILL, SIGINT, SIGQUIT, SIGTERM, SIGALRM, SIGSEGV};
static int MAX_COMPLETIONS = 11;
static char *restore = NULL;

typedef struct _EditorInfo {
  char *filename;
  char *id;
  GList *gl;
  WebKitDOMElement *element;
  const char *tagname;
} EditorInfo;


/* FUNCTION_MAP{{{*/
static FunctionMap FMAP [] = {
  { { "add_view",              "Add a new view",                    }, 1, 
    (Func)commands_add_view,            NULL,                              ALWAYS_SM,     { .p = NULL }, },
  { { "allow_cookie",          "Cookie allowed",                    }, 1, 
    (Func)commands_allow_cookie,        "No new domain in current context",    POST_SM, },
  { { "bookmark",              "Bookmark current page",             }, 1, 
    (Func)commands_bookmark,            NO_URL,                            POST_SM, },
  { { "bookmarks",             "Bookmarks",                         }, 0,
    (Func)commands_bookmarks,           "No Bookmarks",                    POST_SM,     { .n = OPEN_NORMAL }, }, 
  { { "bookmarks_nv",          "Bookmarks new view",                }, 0,
    (Func)commands_bookmarks,           "No Bookmarks",                    NEVER_SM,     { .n = OPEN_NEW_VIEW }, },
  { { "bookmarks_nw",          "Bookmarks new window",              }, 0, 
    (Func)commands_bookmarks,           "No Bookmarks",                    NEVER_SM,     { .n = OPEN_NEW_WINDOW}, }, 
  { { "new_view",              "New view for next navigation",      }, 0, 
    (Func)commands_new_window_or_view,  NULL,                              NEVER_SM,     { .n = OPEN_NEW_VIEW}, }, 
  { { "new_window",            "New window for next navigation",    }, 0, 
    (Func)commands_new_window_or_view,  NULL,                              NEVER_SM,     { .n = OPEN_NEW_WINDOW}, }, 
  { { "command_mode",          "Enter command mode",                }, 0, 
    (Func)commands_command_mode,            NULL,                              POST_SM, },
  { { "decrease_master",       "Decrease master area",              }, 1, 
    (Func)commands_resize_master,       "Cannot decrease further",         ALWAYS_SM,    { .n = 5 } },
  { { "download_hint",         "Download",                          }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_show_hints,          NO_HINTS,                          NEVER_SM,    { .n = OPEN_DOWNLOAD }, },
  { { "find_backward",         "Find backward ",                    }, CP_COMMANDLINE|CP_HAS_MODE, 
    (Func)commands_find,                NO_URL,                            NEVER_SM,     { .b = false }, },
  { { "find_forward",          "Find forward ",                     }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_find,                NO_URL,                            NEVER_SM,     { .b = true }, },
  { { "find_next",             "Find next",                         }, 1, 
    (Func)dwb_search,                  "No matches",                      ALWAYS_SM,     { .b = true }, },
  { { "find_previous",         "Find previous",                     }, 1, 
    (Func)dwb_search,                  "No matches",                      ALWAYS_SM,     { .b = false }, },
  { { "focus_input",           "Focus input",                       }, 1, 
    (Func)commands_focus_input,        "No input found in current context",      ALWAYS_SM, },
  { { "focus_next",            "Focus next view",                   }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_focus,              "No other view",                   ALWAYS_SM,  { .n = 1 } },
  { { "focus_prev",            "Focus previous view",               }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_focus,              "No other view",                   ALWAYS_SM,  { .n = -1 } },
  { { "focus_nth_view",        "Focus nth view",                    }, 0, 
    (Func)commands_focus_nth_view,       "No such view",                   ALWAYS_SM,  { 0 } },
  { { "hint_mode",             "Follow hints",                      }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_show_hints,          NO_HINTS,                          NEVER_SM,    { .n = OPEN_NORMAL }, },
  { { "hint_mode_nv",          "Follow hints (new view)",           }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_show_hints,          NO_HINTS,                          NEVER_SM,    { .n = OPEN_NEW_VIEW }, },
  { { "hint_mode_nw",          "Follow hints (new window)",         }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_show_hints,          NO_HINTS,                          NEVER_SM,    { .n = OPEN_NEW_WINDOW }, },
  { { "history_back",          "Go Back",                           }, 1, 
    (Func)commands_history_back,        "Beginning of History",            ALWAYS_SM, },
  { { "history_forward",       "Go Forward",                        }, 1, 
    (Func)commands_history_forward,     "End of History",                  ALWAYS_SM, },
  { { "increase_master",       "Increase master area",              }, 1, 
    (Func)commands_resize_master,       "Cannot increase further",         ALWAYS_SM,    { .n = -5 } },
  { { "insert_mode",           "Insert Mode",                       }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_insert_mode,             NULL,                              POST_SM, },
  { { "load_html",             "Load html",                         }, 1, 
    (Func)commands_open,           NULL,                       NEVER_SM,   { .i = HTML_STRING, .n = OPEN_NORMAL,      .p = NULL } },
  { { "load_html_nv",          "Load html new view",                }, 1, 
    (Func)commands_open,           NULL,                       NEVER_SM,   { .i = HTML_STRING, .n = OPEN_NEW_VIEW,    .p = NULL } },
  { { "open",                  "open",                              }, 1, 
    (Func)commands_open,                NULL,                 NEVER_SM,   { .n = OPEN_NORMAL,      .p = NULL } },
  { { "Open",                  "open",                              }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_open,                NULL,                 NEVER_SM,   { .n = OPEN_NORMAL | SET_URL, .p = NULL } },
  { { "open_nv",               "tabopen",                          }, 1, 
    (Func)commands_open,                NULL,                 NEVER_SM,   { .n = OPEN_NEW_VIEW,     .p = NULL } },
  { { "Open_nv",               "tabopen",                          }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_open,                NULL,                 NEVER_SM,   { .n = OPEN_NEW_VIEW | SET_URL, .p = NULL } },
  { { "open_nw",               "winopen",                           }, 1, 
    (Func)commands_open,                NULL,                 NEVER_SM,   { .n = OPEN_NEW_WINDOW,     .p = NULL } },
  { { "Open_nw",               "winopen",                           }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_open,                NULL,                 NEVER_SM,   { .n = OPEN_NEW_WINDOW | SET_URL,     .p = NULL } },
  { { "open_quickmark",        "Open quickmark",                         }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_quickmark,           NO_URL,                            NEVER_SM,   { .n = QUICK_MARK_OPEN, .i=OPEN_NORMAL }, },
  { { "open_quickmark_nv",     "Open quickmark in a new tab",                }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_quickmark,           NULL,                              NEVER_SM,    { .n = QUICK_MARK_OPEN, .i=OPEN_NEW_VIEW }, },
  { { "open_quickmark_nw",     "Open quickmark in a new window",              }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_quickmark,           NULL,                              NEVER_SM,    { .n = QUICK_MARK_OPEN, .i=OPEN_NEW_WINDOW }, },
  { { "open_start_page",       "Open startpage",                    }, 1, 
    (Func)commands_open_startpage,      "No startpage set",                ALWAYS_SM, },
  { { "push_master",           "Push to master area",               }, 1, 
    (Func)commands_push_master,         "No other view",                   ALWAYS_SM, },
  { { "quit",           "Quit dwb",               }, 1, 
    (Func)commands_quit,         NULL,                   ALWAYS_SM, },
  { { "reload",                "Reload current page",                            }, 1, 
    (Func)commands_reload,              NULL,                              ALWAYS_SM, },
  { { "reload_bypass_cache",   "Reload current page without using any cached data",  }, 1, 
    (Func)commands_reload_bypass_cache,       NULL,                              ALWAYS_SM, },
  { { "stop_loading",   "Stop loading current page",  }, 1, 
    (Func)commands_stop_loading,       NULL,                              ALWAYS_SM, },
  { { "remove_view",           "Close view",                        }, 1, 
    (Func)commands_remove_view,         NULL,                              ALWAYS_SM, },
  { { "save_quickmark",        "Save a quickmark for this page",    }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_quickmark,           NO_URL,                            NEVER_SM,    { .n = QUICK_MARK_SAVE }, },
  { { "save_search_field",     "Add a new searchengine",            }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_add_search_field,    "No input in current context",     POST_SM, },
  { { "scroll_percent",        "Scroll to percentage",              }, 1, 
    (Func)commands_scroll,              NULL,                              ALWAYS_SM,    { .n = SCROLL_PERCENT }, },
  { { "scroll_bottom",         "Scroll to  bottom of the page",     }, 1, 
    (Func)commands_scroll,              NULL,                              ALWAYS_SM,    { .n = SCROLL_BOTTOM }, },
  { { "scroll_down",           "Scroll down",                       }, 1, 
    (Func)commands_scroll,              "Bottom of the page",              ALWAYS_SM,    { .n = SCROLL_DOWN, }, },
  { { "scroll_left",           "Scroll left",                       }, 1, 
    (Func)commands_scroll,              "Left side of the page",           ALWAYS_SM,    { .n = SCROLL_LEFT }, },
  { { "scroll_halfpage_down",  "Scroll one-half page down",         }, 1, 
    (Func)commands_scroll,              "Bottom of the page",              ALWAYS_SM,    { .n = SCROLL_HALF_PAGE_DOWN, }, },
  { { "scroll_halfpage_up",    "Scroll one-half page up",           }, 1, 
    (Func)commands_scroll,              "Top of the page",                 ALWAYS_SM,    { .n = SCROLL_HALF_PAGE_UP, }, },
  { { "scroll_page_down",      "Scroll one page down",              }, 1, 
    (Func)commands_scroll,              "Bottom of the page",              ALWAYS_SM,    { .n = SCROLL_PAGE_DOWN, }, },
  { { "scroll_page_up",        "Scroll one page up",                }, 1, 
    (Func)commands_scroll,              "Top of the page",                 ALWAYS_SM,    { .n = SCROLL_PAGE_UP, }, },
  { { "scroll_right",          "Scroll left",                       }, 1, 
    (Func)commands_scroll,              "Right side of the page",          ALWAYS_SM,    { .n = SCROLL_RIGHT }, },
  { { "scroll_top",            "Scroll to the top of the page",     }, 1, 
    (Func)commands_scroll,              NULL,                              ALWAYS_SM,    { .n = SCROLL_TOP }, },
  { { "scroll_up",             "Scroll up",                         }, 1, 
    (Func)commands_scroll,              "Top of the page",                 ALWAYS_SM,    { .n = SCROLL_UP, }, },
  { { "set_setting",    "Set setting",               }, 0, 
    (Func)commands_set_setting,         NULL,                              NEVER_SM,    },
  { { "set_key",               "Set keybinding",                    }, CP_COMMANDLINE | CP_HAS_MODE, 
    (Func)commands_set_key,             NULL,                              NEVER_SM,    { 0 } },
  { { "show_keys",             "Key configuration",                 }, 1, 
    (Func)commands_show_keys,           NULL,                              ALWAYS_SM, },
  { { "show_settings",         "Settings configuration",                          }, 1, 
    (Func)commands_show_settings,       NULL,                              ALWAYS_SM,    },
  { { "toggle_bottomstack",    "Toggle bottomstack",                }, 1, 
    (Func)commands_set_orientation,     NULL,                              ALWAYS_SM, },
  { { "toggle_maximized",      "Toggle maximized",                  }, 1, 
    (Func)commands_toggle_maximized,    NULL,                              ALWAYS_SM, },
  { { "view_source",           "View source",                       }, 1, 
    (Func)commands_view_source,         NULL,                              ALWAYS_SM, },
  { { "zoom_in",               "Zoom in",                           }, 1, 
    (Func)commands_zoom_in,             "Cannot zoom in further",          ALWAYS_SM, },
  { { "zoom_normal",           "Zoom to 100%",                         }, 1, 
    (Func)commands_set_zoom_level,      NULL,                              ALWAYS_SM,    { .d = 1.0,   .p = NULL } },
  { { "zoom_out",              "Zoom out",                          }, 1, 
    (Func)commands_zoom_out,            "Cannot zoom out further",         ALWAYS_SM, },
  /* yank and paste */
  { { "yank",                  "Yank current url",                              }, 1, 
    (Func)commands_yank,                 NO_URL,                 POST_SM,  { .p = GDK_NONE } },
  { { "yank_primary",          "Yank current url to Primary selection",         }, 1, 
    (Func)commands_yank,                 NO_URL,                 POST_SM,  { .p = GDK_SELECTION_PRIMARY } },
  { { "paste",                 "Open url from clipboard",                             }, 1, 
    (Func)commands_paste,               "Clipboard is empty",    ALWAYS_SM, { .n = OPEN_NORMAL, .p = GDK_NONE } },
  { { "paste_primary",         "Open url from primary selection",           }, 1, 
    (Func)commands_paste,               "No primary selection",  ALWAYS_SM, { .n = OPEN_NORMAL, .p = GDK_SELECTION_PRIMARY } },
  { { "paste_nv",              "Open url from clipboard in a new tab",                   }, 1, 
    (Func)commands_paste,               "Clipboard is empty",    ALWAYS_SM, { .n = OPEN_NEW_VIEW, .p = GDK_NONE } },
  { { "paste_primary_nv",      "Open url from primary selection in a new window", }, 1, 
    (Func)commands_paste,               "No primary selection",  ALWAYS_SM, { .n = OPEN_NEW_VIEW, .p = GDK_SELECTION_PRIMARY } },
  { { "paste_nw",              "Open url from clipboard in a new window",                   }, 1, 
    (Func)commands_paste,             "Clipboard is empty",    ALWAYS_SM, { .n = OPEN_NEW_WINDOW, .p = GDK_NONE } },
  { { "paste_primary_nw",      "Open url from primary selection in a new window", }, 1, 
    (Func)commands_paste,             "No primary selection",  ALWAYS_SM, { .n = OPEN_NEW_WINDOW, .p = GDK_SELECTION_PRIMARY } },

  { { "save_session",          "Save current session", },              1, 
    (Func)commands_save_session,        NULL,                              ALWAYS_SM,  { .n = NORMAL_MODE } },
  { { "save_named_session",    "Save current session with name", },    CP_COMMANDLINE|CP_HAS_MODE, 
    (Func)commands_save_session,        NULL,                              POST_SM,  { .n = SAVE_SESSION } },
  { { "save",                  "Save all configuration files", },      1, 
    (Func)commands_save_files,        NULL,                              POST_SM,  { .n = SAVE_SESSION } },
  { { "undo",                  "Undo closing last tab", },             1, 
    (Func)commands_undo,              "No more closed views",                              POST_SM },
  { { "web_inspector",         "Open the webinspector", },             1, 
    (Func)commands_web_inspector,              "Enable developer extras for the webinspector",                              POST_SM },
  { { "reload_scripts",         "Reload scripts", },             1, 
    (Func)commands_reload_scripts,              NULL,                              ALWAYS_SM },

  /* Entry editing */
  { { "entry_delete_word",      "Command line: Delete word in", },                      0,  
    (Func)commands_entry_delete_word,            NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_delete_letter",    "Command line: Delete a single letter", },           0,  
    (Func)commands_entry_delete_letter,          NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_delete_line",      "Command line: Delete to beginning of the line", },  0,  
    (Func)commands_entry_delete_line,            NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_word_forward",     "Command line: Move cursor forward on word", },      0,  
    (Func)commands_entry_word_forward,           NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_word_back",        "Command line: Move cursor back on word", },         0,  
    (Func)commands_entry_word_back,              NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_history_back",     "Command line: Command history back", },             0,  
    (Func)commands_entry_history_back,           NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "entry_history_forward",  "Command line: Command history forward", },          0,  
    (Func)commands_entry_history_forward,        NULL,        ALWAYS_SM,  { 0 }, true, }, 
  { { "download_set_execute",   "Downloads: toggle between spawning application/download path", },                0, 
    (Func)download_set_execute,        NULL,       ALWAYS_SM,  { 0 }, true, }, 
  { { "complete_history",       "Complete browsing history", },       0, 
    (Func)commands_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_HISTORY }, true, }, 
  { { "complete_bookmarks",     "Complete bookmarks", },              0, 
    (Func)commands_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_BOOKMARK }, true, }, 
  { { "complete_commands",      "Complete command history", },        0, 
    (Func)commands_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_INPUT }, true, }, 
  { { "complete_searchengines", "Complete searchengines", },          0, 
    (Func)commands_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_SEARCH }, true, }, 
  { { "complete_userscript",    "Complete userscripts", },            0, 
    (Func)commands_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_USERSCRIPT }, true, }, 
  { { "complete_path",          "Complete local file path", },        0, 
    (Func)commands_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_PATH }, true, }, 
  { { "complete_current_history",          "Complete history of current tab", },        0, 
    (Func)commands_complete_type,             NULL,     ALWAYS_SM,     { .n = COMP_CUR_HISTORY }, true, }, 
  { { "buffers",                          "Buffer", },        CP_COMMANDLINE | CP_HAS_MODE,
    (Func)commands_complete_type,            "Only one buffer",     POST_SM,     { .n = COMP_BUFFER }, }, 

  { { "spell_checking",        "Setting: spell checking",         },   0, 
    (Func)commands_toggle_property,     NULL,                              POST_SM,    { .p = "enable-spell-checking" } },
  { { "scripts",               "Setting: scripts",                },   1, 
    (Func)commands_toggle_property,     NULL,                              POST_SM,    { .p = "enable-scripts" } },
  { { "auto_shrink_images",    "Toggle autoshrink images",        },   0, 
    (Func)commands_toggle_property,     NULL,                    POST_SM,    { .p = "auto-shrink-images" } },
  { { "autoload_images",       "Toggle autoload images",          },   0, 
    (Func)commands_toggle_property,     NULL,                    POST_SM,    { .p = "auto-load-images" } },
  { { "autoresize_window",     "Toggle autoresize window",        },   0, 
    (Func)commands_toggle_property,     NULL,                    POST_SM,    { .p = "auto-resize-window" } },
  { { "caret_browsing",        "Toggle caret browsing",           },   0, 
    (Func)commands_toggle_property,     NULL,                    POST_SM,    { .p = "enable-caret-browsing" } },
  { { "default_context_menu",  "Toggle enable default context menu",           }, 0, 
    (Func)commands_toggle_property,     NULL,       POST_SM,    { .p = "enable-default-context-menu" } },
  { { "file_access_from_file_uris",     "Toggle file access from file uris",   }, 0, 
    (Func)commands_toggle_property,     NULL,                  POST_SM, { .p = "enable-file-acces-from-file-uris" } },
  { { "universal file_access_from_file_uris",   "Toggle universal file access from file uris",   }, 0, 
    (Func)commands_toggle_property,  NULL,   POST_SM, { .p = "enable-universal-file-acces-from-file-uris" } },
  { { "java_applets",          "Toggle java applets",             }, 0, 
    (Func)commands_toggle_property,     NULL,                    POST_SM,    { .p = "enable-java-applets" } },
  { { "plugins",               "Toggle plugins",                  }, 1, 
    (Func)commands_toggle_property,     NULL,                    POST_SM,    { .p = "enable-plugins" } },
  { { "private_browsing",      "Toggle private browsing",         }, 0, 
    (Func)commands_toggle_property,     NULL,                    POST_SM,    { .p = "enable-private-browsing" } },
  { { "page_cache",            "Toggle page cache",               }, 0, 
    (Func)commands_toggle_property,     NULL,                    POST_SM,    { .p = "enable-page-cache" } },
  { { "js_can_open_windows",   "Toggle Javascript can open windows automatically", }, 0, 
    (Func)commands_toggle_property,     NULL,   POST_SM,    { .p = "javascript-can-open-windows-automatically" } },
  { { "enforce_96_dpi",        "Toggle enforce a resolution of 96 dpi", },    0, 
    (Func)commands_toggle_property,     NULL,           POST_SM,    { .p = "enforce-96-dpi" } },
  { { "print_backgrounds",     "Toggle print backgrounds", },      0,    
    (Func)commands_toggle_property,    NULL,                    POST_SM,    { .p = "print-backgrounds" } },
  { { "resizable_text_areas",  "Toggle resizable text areas", },   0,  
    (Func)commands_toggle_property,      NULL,                    POST_SM,    { .p = "resizable-text-areas" } },
  { { "tab_cycle",             "Toggle tab cycles through elements", },  0,   
    (Func)commands_toggle_property,     NULL,              POST_SM,    { .p = "tab-key-cycles-through-elements" } },
  { { "proxy",                 "Toggle proxy",                    },        1,     
    (Func)commands_toggle_proxy,        NULL,                    POST_SM,    { 0 } },
  { { "toggle_scripts_host", "Toggle block content for current host" },   1, 
    (Func) commands_toggle_scripts, NULL,                  POST_SM,    { .n = ALLOW_HOST } }, 
  { { "toggle_scripts_uri",    "Toggle block content for current url" }, 1, 
    (Func) commands_toggle_scripts, NULL,                POST_SM,    { .n = ALLOW_URI } }, 
  { { "toggle_scripts_host_tmp", "Toggle block content for current host for this session" },  1, 
    (Func) commands_toggle_scripts, NULL,      POST_SM,    { .n = ALLOW_HOST | ALLOW_TMP } }, 
  { { "toggle_scripts_uri_tmp", "Toggle block content for current url for this session" },   1, 
    (Func) commands_toggle_scripts, NULL,       POST_SM,    { .n = ALLOW_URI | ALLOW_TMP } }, 
  { { "toggle_plugins_host", "Toggle plugin blocker for current host" },   1, 
    (Func) commands_toggle_plugin_blocker, NULL,                  POST_SM,    { .n = ALLOW_HOST } }, 
  { { "toggle_plugins_uri",    "Toggle plugin blocker for current url" }, 1, 
    (Func) commands_toggle_plugin_blocker, NULL,                POST_SM,    { .n = ALLOW_URI } }, 
  { { "toggle_plugins_host_tmp", "Toggle block content for current domain for this session" },  1, 
    (Func) commands_toggle_plugin_blocker, NULL,      POST_SM,    { .n = ALLOW_HOST | ALLOW_TMP } }, 
  { { "toggle_plugins_uri_tmp", "Toggle block content for current url for this session" },   1, 
    (Func) commands_toggle_plugin_blocker, NULL,       POST_SM,    { .n = ALLOW_URI | ALLOW_TMP } }, 
  { { "toggle_hidden_files",   "Toggle hidden files in directory listing" },  1, 
    (Func) commands_toggle_hidden_files, NULL,                  ALWAYS_SM,    { 0 } }, 
  { { "print",                 "Print current page" },                         1, 
    (Func) commands_print, NULL,                             POST_SM,    { 0 } }, 
  { { "execute_userscript",    "Execute userscript" },                 1, 
    (Func) commands_execute_userscript, "No userscripts available",     NEVER_SM,    { 0 } }, 
  { { "fullscreen",    "Toggle fullscreen" },                 1, 
    (Func) commands_fullscreen, NULL,     ALWAYS_SM,    { 0 } }, 
  { { "pass_through",    "Pass-through mode" },                 1, 
    (Func) commands_pass_through, NULL,     POST_SM,    { 0 } }, 
  { { "open_editor",    "Open external editor" },                 1, 
    (Func) commands_open_editor, "No input focused",     NEVER_SM,    { 0 } }, 
};/*}}}*/

/* DWB_SETTINGS {{{*/
/* SETTINGS_ARRAY {{{*/
  /* { name,    description, builtin, global, type,  argument,  set-function */
static WebSettings DWB_SETTINGS[] = {
  { { "auto-load-images",			                   "Load images automatically", },                                         
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "auto-resize-window",			                 "Autoresize window", },                                       
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "auto-shrink-images",			                 "Automatically shrink standalone images to fit", },                                       
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "cursive-font-family",			               "Cursive font family used to display text", },                                     
    SETTING_BUILTIN,  CHAR,    { .p = "serif"           }, (S_Func) dwb_webkit_setting,  },
  { { "default-encoding",			                   "Default encoding used to display text", },                                        
    SETTING_BUILTIN,  CHAR,    { .p = NULL      }, (S_Func) dwb_webkit_setting,  },
  { { "default-font-family",			               "Default font family used to display text", },                                     
    SETTING_BUILTIN,  CHAR,    { .p = "sans-serif"      }, (S_Func) dwb_webkit_setting,  },
  { { "default-font-size",			                 "Default font size used to display text", },                                       
    SETTING_BUILTIN,  INTEGER, { .i = 12                }, (S_Func) dwb_webkit_setting,  },
  { { "default-monospace-font-size",			       "Default monospace font size used to display text", },                             
    SETTING_BUILTIN,  INTEGER, { .i = 10                }, (S_Func) dwb_webkit_setting,  },
  { { "enable-caret-browsing",			             "Whether to enable caret browsing", },                                          
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-default-context-menu",			       "Whether to enable the right click context menu", },                             
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enable-developer-extras",			           "Whether developer extensions should be enabled",    },                              
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-dns-prefetching",			           "Whether webkit prefetches domain names",    },                              
    SETTING_BUILTIN,  BOOLEAN, { .b = true             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-dom-paste",			                   "Whether to enable DOM paste", },                                        
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-frame-flattening",			           "Whether to enable Frame Flattening", },                                        
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-file-access-from-file-uris",			 "Whether file access from file uris is allowed", },                              
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enable-html5-database",			             "Enable HTML5 client side SQL-database support" },                                    
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enable-html5-local-storage",			         "Enable HTML5 local storage", },                              
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enable-java-applet",			                 "Whether to enable java applets", },                                            
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enable-offline-web-application-cache",		 "Enable HTML5 offline web application cache", },                           
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enable-page-cache",			                 "Whether to enable page cache", },                                              
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-plugins",			                     "Whether to enable plugins", },                                                 
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enable-private-browsing",			           "Whether to enable private browsing mode", },                                        
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_set_private_browsing,  },
  { { "enable-scripts",			                     "Enable embedded scripting languages", },                                                  
    SETTING_PER_VIEW,  BOOLEAN, { .b = true              }, (S_Func) dwb_set_scripts,  },
  { { "enable-site-specific-quirks",			       "Enable site-specific compatibility workarounds", },                                    
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-spatial-navigation",			         "Spatial navigation", },                                      
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-spell-checking",			             "Whether to enable spell checking", },                                          
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "enable-universal-access-from-file-uris",	 "Whether to allow files loaded through file:", },                        
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enable-xss-auditor",			                 "Whether to enable the XSS auditor", },                                             
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "enforce-96-dpi",			                     "Enforce a resolution of 96 dpi", },                                          
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "fantasy-font-family",			               "Default fantasy font family used to display text", },                                     
    SETTING_BUILTIN,  CHAR,    { .p = "serif"           }, (S_Func) dwb_webkit_setting,  },
  { { "javascript-can-access-clipboard",			   "Whether javascript can access clipboard", },                         
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "javascript-can-open-windows-automatically", "Whether javascript can open windows", },             
    SETTING_BUILTIN,  BOOLEAN, { .b = false             }, (S_Func) dwb_webkit_setting,  },
  { { "minimum-font-size",			                 "Minimum font size to display text", },                                       
    SETTING_BUILTIN,  INTEGER, { .i = 5                 }, (S_Func) dwb_webkit_setting,  },
  { { "minimum-logical-font-size",			         "Minimum logical font size used to display text", },                               
    SETTING_BUILTIN,  INTEGER, { .i = 5                 }, (S_Func) dwb_webkit_setting,  },
  { { "monospace-font-family",			             "Monospace font family used to display text", },                                   
    SETTING_BUILTIN,  CHAR,    { .p = "monospace"       }, (S_Func) dwb_webkit_setting,  },
  { { "print-backgrounds",			                 "Whether background images should be printed", },                                       
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "resizable-text-areas",			               "Whether text areas are resizable", },                                    
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "sans-serif-font-family",			             "Sans serif font family used to display text", },                                  
    SETTING_BUILTIN,  CHAR,    { .p = "sans-serif"      }, (S_Func) dwb_webkit_setting,  },
  { { "serif-font-family",			                 "Serif font family used to display text", },                                       
    SETTING_BUILTIN,  CHAR,    { .p = "serif"           }, (S_Func) dwb_webkit_setting,  },
  { { "spell-checking-languages",			           "Language used for spellchecking sperated by commas", },                                
    SETTING_BUILTIN,  CHAR,    { .p = NULL              }, (S_Func) dwb_webkit_setting,  },
  { { "tab-key-cycles-through-elements",			   "Tab cycles through elements in insert mode", },              
    SETTING_BUILTIN,  BOOLEAN, { .b = true              }, (S_Func) dwb_webkit_setting,  },
  { { "user-agent",			                         "The user agent string", },                                              
    SETTING_PER_VIEW,                CHAR,    { .p = NULL              }, (S_Func) dwb_set_user_agent,  },
  { { "user-stylesheet-uri",			               "The uri of a stylsheet applied to every page", },                                     
    SETTING_BUILTIN,  CHAR,    { .p = NULL              }, (S_Func) dwb_webkit_setting,  },
  { { "zoom-step",			                         "The zoom step", },                                               
    SETTING_BUILTIN,  DOUBLE,  { .d = 0.1               }, (S_Func) dwb_webkit_setting,  },
  { { "custom-encoding",                         "The custom encoding of the view", },                                         
    SETTING_PER_VIEW,                CHAR,    { .p = NULL           }, (S_Func) dwb_webview_property,  },
  { { "editable",                                "Whether content can be modified", },                                        
    SETTING_PER_VIEW,                BOOLEAN, { .b = false             }, (S_Func) dwb_webview_property,  },
  { { "full-content-zoom",                       "Whether the full content is scaled when zooming", },                                       
    SETTING_PER_VIEW,                BOOLEAN, { .b = false             }, (S_Func) dwb_webview_property,  },
  { { "zoom-level",                              "The default zoom level", },
    SETTING_PER_VIEW,                DOUBLE,  { .d = 1.0               }, (S_Func) dwb_webview_property,  },
  { { "proxy",                                   "Whether to use a HTTP-proxy", },                                              
    SETTING_GLOBAL,      BOOLEAN, { .b = false              },  (S_Func) dwb_set_proxy,  },
  { { "proxy-url",                               "The HTTP-proxy url", },                                          
    SETTING_GLOBAL,      CHAR,    { .p = NULL              },   (S_Func) dwb_soup_init_proxy,  },
  { { "ssl-strict",                               "Whether to allow only save certificates", },                                          
    SETTING_GLOBAL,      BOOLEAN,    { .b = true            },   (S_Func) dwb_soup_init_session_features,  },
  { { "ssl-ca-cert",                               "Path to ssl-certificate", },                                          
    SETTING_GLOBAL,      CHAR,    { .p = NULL            },   (S_Func) dwb_soup_init_session_features,  },
  { { "cookies",                                  "Whether to allow all cookies", },                                     
    SETTING_GLOBAL,      BOOLEAN, { .b = false             }, (S_Func) dwb_init_vars,  },
  { { "background-tabs",			                     "Whether to open tabs in background", },                                 
    SETTING_GLOBAL,      BOOLEAN,    { .b = false         }, (S_Func) dwb_set_background_tab,  },
  { { "scroll-step",			                     "Whether to open tabs in background", },                                 
    SETTING_GLOBAL,      DOUBLE,    { .d = 0         }, (S_Func) dwb_init_vars,  },

  { { "active-fg-color",                         "Foreground color of the active view", },                              
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#ffffff"         },    (S_Func) dwb_reload_layout,   },
  { { "active-bg-color",                         "Background color of the active view", },                              
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#000000"         },    (S_Func) dwb_reload_layout,  },
  { { "normal-fg-color",                         "Foreground color of inactive views", },                            
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#cccccc"         },    (S_Func) dwb_reload_layout,  },
  { { "normal-bg-color",                         "Background color of inactive views", },                            
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#505050"         },    (S_Func) dwb_reload_layout,  },

  { { "tab-active-fg-color",                     "Foreground color of the active tab", },                           
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#ffffff"         },    (S_Func) dwb_reload_layout,  },
  { { "tab-active-bg-color",                     "Background color of the active tab", },                           
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#000000"         },    (S_Func) dwb_reload_layout,  },
  { { "tab-normal-fg-color",                     "Foreground color of inactive tabs", },                         
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#cccccc"         },    (S_Func) dwb_reload_layout,  },
  { { "tab-normal-bg-color",                     "Background color of inactive tabs", },                         
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#505050"         },    (S_Func) dwb_reload_layout,  },
  { { "tab-number-color",                        "Color of the number in the tab", },                      
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#7ac5cd"         },    (S_Func) dwb_reload_layout,  },
  { { "hide-tabbar",                             "Whether to hide the tabbar (never, always, tiled)", },                      
    SETTING_GLOBAL,  CHAR,      { .p = "never"         },      (S_Func) dwb_set_hide_tabbar,  },
  { { "tabbed-browsing",                         "Whether to enable tabbed browsing", },                                  
    SETTING_GLOBAL,  BOOLEAN,      { .b = true         },      (S_Func) dwb_set_dummy,  },
  { { "sync-history",                            "Interval to save history to hdd or 0 to directly write to hdd", },                                  
    SETTING_GLOBAL|SETTING_ONINIT,  INTEGER,      { .i = 0         },      (S_Func) dwb_set_sync_interval,  },

  { { "active-completion-fg-color",                    "Foreground color of the active tabcompletion item", },                        
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#53868b"         }, (S_Func) dwb_init_style,  },
  { { "active-completion-bg-color",                    "Background color of the active tabcompletion item", },                        
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#000000"         }, (S_Func) dwb_init_style,  },
  { { "normal-completion-fg-color",                    "Foreground color of an inactive tabcompletion item", },                      
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#eeeeee"         }, (S_Func) dwb_init_style,  },
  { { "normal-completion-bg-color",                    "Background color of an inactive tabcompletion item", },                      
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#151515"         }, (S_Func) dwb_init_style,  },

  { { "ssl-trusted-color",                         "Color for ssl-encrypted sites, trusted certificate", },                 
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#00ff00"         }, (S_Func) dwb_init_style,  },
  { { "ssl-untrusted-color",                       "Color for ssl-encrypted sites, untrusted certificate", },                 
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#ff0000"         }, (S_Func) dwb_init_style,  },
  { { "error-color",                             "Color for error messages", },                                         
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#ff0000"         }, (S_Func) dwb_init_style,  },
  { { "status-allowed-color",                        "Color of allowed elements in the statusbar", },           
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#00ff00"       },    (S_Func) dwb_reload_layout,  },
  { { "status-blocked-color",                        "Color of blocked elements in the statusbar", },           
    SETTING_GLOBAL,  COLOR_CHAR, { .p = "#ffffff"       },    (S_Func) dwb_reload_layout,  },

  { { "font",                                    "Default font used for the ui", },                                       
    SETTING_GLOBAL,  CHAR, { .p = "monospace 8"          },   (S_Func) dwb_reload_layout,  },
  { { "font-inactive",                           "Font of views without focus", },                  
    SETTING_GLOBAL,  CHAR, { .p = NULL                   },   (S_Func) dwb_reload_layout,  },
  { { "font-entry",                              "Font of the addressbar", },                            
    SETTING_GLOBAL,  CHAR, { .p = NULL                   },   (S_Func) dwb_reload_layout,  },
  { { "font-completion",                         "Font for tab-completion", },                            
    SETTING_GLOBAL,  CHAR, { .p = NULL                   },   (S_Func) dwb_reload_layout,  },
   
  { { "hint-letter-seq",                       "Letter sequence for letter hints", },             
    SETTING_GLOBAL,  CHAR, { .p = "FDSARTGBVECWXQYIOPMNHZULKJ"  }, (S_Func) dwb_reload_scripts,  },
  { { "hint-highlight-links",                  "Whether to highlight links in hintmode", },             
    SETTING_GLOBAL,  BOOLEAN, { .b = false  }, (S_Func) dwb_reload_scripts,  },
  { { "hint-style",                              "Whether to use 'letter' or 'number' hints", },                     
    SETTING_GLOBAL,  CHAR, { .p = "letter"            },     (S_Func) dwb_reload_scripts,  },
  { { "hint-font",                          "Font size of hints", },                                        
    SETTING_GLOBAL,  CHAR, { .p = "bold 10px monospace"             },     (S_Func) dwb_reload_scripts,  },
  { { "hint-fg-color",                           "Foreground color of hints", },                                 
    SETTING_GLOBAL,  CHAR, { .p = "#000000"      },     (S_Func) dwb_reload_scripts,  },
  { { "hint-bg-color",                           "Background color of hints", },                                 
    SETTING_GLOBAL,  CHAR, { .p = "#ffffff"      },     (S_Func) dwb_reload_scripts,  },
  { { "hint-active-color",                       "Color of the active link in hintmode", },                                
    SETTING_GLOBAL,  CHAR, { .p = "#00ff00"      },     (S_Func) dwb_reload_scripts,  },
  { { "hint-normal-color",                       "Color of inactive links in hintmode", },                              
    SETTING_GLOBAL,  CHAR, { .p = "#ffff99"      },     (S_Func) dwb_reload_scripts,  },
  { { "hint-border",                             "Border used for hints", },                                      
    SETTING_GLOBAL,  CHAR, { .p = "1px solid #000000"    }, (S_Func) dwb_reload_scripts,  },
  { { "hint-opacity",                            "The opacity of hints", },                                     
    SETTING_GLOBAL,  DOUBLE, { .d = 0.8         },          (S_Func) dwb_reload_scripts,  },
  { { "auto-completion",                         "Show possible shortcuts", },                                
    SETTING_GLOBAL,  BOOLEAN, { .b = true         },     (S_Func)completion_set_autcompletion,  },
  { { "startpage",                               "The default homepage", },                                        
    SETTING_GLOBAL,  CHAR,    { .p = "dwb://bookmarks" },        (S_Func)dwb_set_startpage,  }, 
  { { "single-instance",                         "Whether to have only on instance", },                                         
    SETTING_GLOBAL,  BOOLEAN,    { .b = false },          (S_Func)dwb_set_single_instance,  }, 
  { { "save-session",                            "Whether to automatically save sessions", },                                       
    SETTING_GLOBAL,  BOOLEAN,    { .b = false },          (S_Func)dwb_set_dummy,  }, 
  

  /* downloads */
  { { "download-external-command",                        "External program used for downloads", },                               
    SETTING_GLOBAL,  CHAR, { .p = "xterm -e wget 'dwb_uri' -O 'dwb_output' --load-cookies 'dwb_cookies'"   },     (S_Func)dwb_set_dummy,  },
  { { "download-use-external-program",           "Whether to use an external download program", },                           
    SETTING_GLOBAL,  BOOLEAN, { .b = false         },    (S_Func)dwb_set_dummy,  },

  { { "complete-history",                        "Whether to complete browsing history with tab", },                              
    SETTING_GLOBAL,  BOOLEAN, { .b = true         },     (S_Func)dwb_init_vars,  },
  { { "complete-bookmarks",                        "Whether to complete bookmarks with tab", },                                     
    SETTING_GLOBAL,  BOOLEAN, { .b = true         },     (S_Func)dwb_init_vars,  },
  { { "complete-searchengines",                   "Whether to complete searchengines with tab", },                                     
    SETTING_GLOBAL,  BOOLEAN, { .b = false         },     (S_Func)dwb_init_vars,  },
  { { "complete-commands",                        "Whether to complete the command history", },                                     
    SETTING_GLOBAL,  BOOLEAN, { .b = true         },     (S_Func)dwb_init_vars,  },
  { { "complete-userscripts",                        "Whether to complete userscripts", },                                     
    SETTING_GLOBAL,  BOOLEAN, { .b = false         },     (S_Func)dwb_init_vars,  },

  { { "use-fifo",                        "Create a fifo pipe for communication", },                            
    SETTING_GLOBAL,  BOOLEAN, { .b = false         },     (S_Func)dwb_set_dummy,  },
    
  { { "default-width",                           "Default width of the window", },                                           
    SETTING_GLOBAL,  INTEGER, { .i = 800          }, (S_Func)dwb_set_dummy,  },
  { { "default-height",                          "Default height of the window", },                                           
    SETTING_GLOBAL,  INTEGER, { .i = 600          }, (S_Func)dwb_set_dummy,  },
  { { "message-delay",                           "Time in seconds, messages are shown", },                                           
    SETTING_GLOBAL,  INTEGER, { .i = 2          }, (S_Func) dwb_set_message_delay,  },
  { { "history-length",                          "Length of the browsing history", },                                          
    SETTING_GLOBAL,  INTEGER, { .i = 500          }, (S_Func) dwb_set_history_length,  },
  { { "size",                                    "Tiling area size in percent", },                     
    SETTING_GLOBAL,  INTEGER, { .i = 30          }, (S_Func)dwb_set_dummy,  },
  { { "factor",                                  "Zoom factor of the tiling area", },                  
    SETTING_GLOBAL,  DOUBLE, { .d = 0.3          }, (S_Func)dwb_set_dummy,  },
  { { "layout",                                  "The default layout (Normal, Bottomstack, Maximized)", },     
    SETTING_GLOBAL,  CHAR, { .p = "Normal MAXIMIZED" },  (S_Func)dwb_set_dummy,  },
  { { "top-statusbar",                                  "Whether to have the statusbar on top", },     
    SETTING_GLOBAL,  BOOLEAN, { .b = false },  (S_Func)dwb_set_dummy,  },
  { { "mail-client",                            "Program used for mailto:-urls", },                                            
    SETTING_GLOBAL,  CHAR, { .p = "xterm -e mutt 'dwb_uri'" }, (S_Func)dwb_set_dummy,  }, 
  { { "ftp-client",                            "Program used for ftp", },                                            
    SETTING_GLOBAL,  CHAR, { .p = "xterm -e ncftp 'dwb_uri'" }, (S_Func)dwb_set_dummy,   }, 
  { { "editor",                            "External editor", },                                            
    SETTING_GLOBAL,  CHAR, { .p = "xterm -e vim dwb_uri" }, (S_Func)dwb_set_dummy,   }, 
  { { "adblocker",                               "Whether to block advertisements via a filterlist", },                   
    SETTING_PER_VIEW,  BOOLEAN, { .b = false }, (S_Func)dwb_set_adblock,   }, 
  { { "plugin-blocker",                         "Whether to block flash plugins and replace them with a clickable element", },                   
    SETTING_PER_VIEW,  BOOLEAN, { .b = true }, (S_Func)dwb_set_plugin_blocker,   }, 
};/*}}}*/

/* SETTINGS_FUNCTIONS{{{*/
/* dwb_set_dummy{{{*/
static void
dwb_set_dummy(GList *gl, WebSettings *s) {
  return;
}/*}}}*/

/* dwb_set_adblock {{{*/
static void
dwb_set_plugin_blocker(GList *gl, WebSettings *s) {
  View *v = gl->data;
  if (s->arg.b) {
    plugins_connect(gl);
    v->status->pb_status ^= (v->status->pb_status & PLUGIN_STATUS_DISABLED) | PLUGIN_STATUS_ENABLED;
  }
  else {
    plugins_disconnect(gl);
    v->status->pb_status ^= (v->status->pb_status & PLUGIN_STATUS_ENABLED) | PLUGIN_STATUS_DISABLED;
  }
}/*}}}*/

/* dwb_set_adblock {{{*/
static void
dwb_set_adblock(GList *gl, WebSettings *s) {
  View *v = gl->data;
  v->status->adblocker = s->arg.b;
}/*}}}*/

/* dwb_set_private_browsing  */
static void
dwb_set_private_browsing(GList *gl, WebSettings *s) {
  dwb.misc.private_browsing = s->arg.b;
  dwb_webkit_setting(gl, s);
}/*}}}*/

/* dwb_set_hide_tabbar{{{*/
static void
dwb_set_hide_tabbar(GList *gl, WebSettings *s) {
  dwb.state.tabbar_visible = dwb_eval_tabbar_visible(s->arg.p);
  dwb_toggle_tabbar();
}/*}}}*/

/* dwb_set_sync_interval{{{*/
static void
dwb_set_sync_interval(GList *gl, WebSettings *s) {
  if (dwb.misc.synctimer > 0) {
    g_source_remove(dwb.misc.synctimer);
    dwb.misc.synctimer = 0;
  }

  if (s->arg.i > 0) {
    dwb.misc.synctimer = g_timeout_add_seconds(s->arg.i, dwb_sync_history, NULL);
  }
}/*}}}*/

/* dwb_set_startpage(GList *l, WebSettings *){{{*/
static void 
dwb_set_startpage(GList *l, WebSettings *s) {
  dwb.misc.startpage = s->arg.p;
}/*}}}*/

/* dwb_set_message_delay(GList *l, WebSettings *){{{*/
static void 
dwb_set_message_delay(GList *l, WebSettings *s) {
  dwb.misc.message_delay = s->arg.i;
}/*}}}*/

/* dwb_set_history_length(GList *l, WebSettings *){{{*/
static void 
dwb_set_history_length(GList *l, WebSettings *s) {
  dwb.misc.history_length = s->arg.i;
}/*}}}*/

/* dwb_set_background_tab (GList *, WebSettings *s) {{{*/
void 
dwb_set_background_tab(GList *l, WebSettings *s) {
  dwb.state.background_tabs = s->arg.b;
}/*}}}*/

/* dwb_set_single_instance(GList *l, WebSettings *s){{{*/
void
dwb_set_single_instance(GList *l, WebSettings *s) {
  if (!s->arg.b) {
    if (dwb.misc.si_channel) {
      g_io_channel_shutdown(dwb.misc.si_channel, true, NULL);
      g_io_channel_unref(dwb.misc.si_channel);
      dwb.misc.si_channel = NULL;
    }
  }
  else if (!dwb.misc.si_channel) {
    dwb_open_si_channel();
  }
}/*}}}*/

/* dwb_set_proxy{{{*/
void
dwb_set_proxy(GList *l, WebSettings *s) {
  if (s->arg.b) {
    SoupURI *uri = soup_uri_new(dwb.misc.proxyuri);
    g_object_set(dwb.misc.soupsession, "proxy-uri", uri, NULL);
    soup_uri_free(uri);
  }
  else  {
    g_object_set(dwb.misc.soupsession, "proxy-uri", NULL, NULL);
  }
  dwb_set_normal_message(dwb.state.fview, true, "Set setting proxy: %s", s->arg.b ? "true" : "false");
}/*}}}*/

void
dwb_set_scripts(GList *gl, WebSettings *s) {
  dwb_webkit_setting(gl, s);
  View *v = VIEW(gl);
  if (s->arg.b) 
    v->status->scripts = SCRIPTS_ALLOWED;
  else 
    v->status->scripts = SCRIPTS_BLOCKED;
}
void
dwb_set_user_agent(GList *gl, WebSettings *s) {
  char *ua = s->arg.p;
  if (! ua) {
    char *current_ua;
    g_object_get(dwb.state.web_settings, "user-agent", &current_ua, NULL);
    s->arg.p = g_strdup_printf("%s %s/%s", current_ua, NAME, VERSION);
  }
  dwb_webkit_setting(gl, s);
  g_hash_table_insert(dwb.settings, g_strdup("user-agent"), s);
}

/* dwb_webkit_setting(GList *gl WebSettings *s) {{{*/
static void
dwb_webkit_setting(GList *gl, WebSettings *s) {
  WebKitWebSettings *settings = gl ? webkit_web_view_get_settings(WEBVIEW(gl)) : dwb.state.web_settings;
  switch (s->type) {
    case DOUBLE:  g_object_set(settings, s->n.first, s->arg.d, NULL); break;
    case INTEGER: g_object_set(settings, s->n.first, s->arg.i, NULL); break;
    case BOOLEAN: g_object_set(settings, s->n.first, s->arg.b, NULL); break;
    case CHAR:    g_object_set(settings, s->n.first, !s->arg.p || !strcmp(s->arg.p, "null") ? NULL : (char*)s->arg.p  , NULL); break;
    default: return;
  }
}/*}}}*/

/* dwb_webview_property(GList, WebSettings){{{*/
static void
dwb_webview_property(GList *gl, WebSettings *s) {
  WebKitWebView *web = gl ? WEBVIEW(gl) : CURRENT_WEBVIEW();
  switch (s->type) {
    case DOUBLE:  g_object_set(web, s->n.first, s->arg.d, NULL); break;
    case INTEGER: g_object_set(web, s->n.first, s->arg.i, NULL); break;
    case BOOLEAN: g_object_set(web, s->n.first, s->arg.b, NULL); break;
    case CHAR:    g_object_set(web, s->n.first, (char*)s->arg.p, NULL); break;
    default: return;
  }
}/*}}}*/

/*dwb_reload_scripts {{{  */
static void 
dwb_reload_scripts(GList *gl, WebSettings *s) {
  dwb_init_scripts();
} /*}}}*//*}}}*/
/*}}}*/


/* CALLBACKS {{{*/

/* dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {{{*/
static gboolean 
dwb_key_press_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  gboolean ret = false;
  Mode mode = CLEAN_MODE(dwb.state.mode);

  PRINT_DEBUG("%d %d %d\n", mode, CLEAN_MODE(mode) & COMPLETE_BUFFER, mode & PASS_THROUGH);
  char *key = util_keyval_to_char(e->keyval);
  if (e->keyval == GDK_KEY_Escape) {
    dwb_change_mode(NORMAL_MODE, true);
    ret = false;
  }
  else if (mode & PASS_THROUGH) {
    ret = false;
  }
  else if (mode == INSERT_MODE) {
    if (CLEAN_STATE(e) & GDK_MODIFIER_MASK) {
      dwb_eval_key(e);
      ret = false;
    }
    else if (e->keyval == GDK_KEY_Return) {
      dwb_change_mode(NORMAL_MODE, true);
    }
  }
  else if (mode == QUICK_MARK_SAVE) {
    if (key) {
      dwb_save_quickmark(key);
    }
    ret = true;
  }
  else if (mode == QUICK_MARK_OPEN) {
    if (key) {
      dwb_open_quickmark(key);
    }
    ret = true;
  }
  else if (gtk_widget_has_focus(dwb.gui.entry) || mode & COMPLETION_MODE) {
    ret = false;
  }
  else if (webkit_web_view_has_selection(CURRENT_WEBVIEW()) && e->keyval == GDK_KEY_Return) {
    dwb_follow_selection();
  }
  else if (DWB_TAB_KEY(e)) {
    completion_autocomplete(dwb.keymap, e);
    ret = true;
  }
  else {
    if (mode & AUTO_COMPLETE) {
      if (DWB_TAB_KEY(e)) {
        completion_autocomplete(NULL, e);
      }
      else if (e->keyval == GDK_KEY_Return) {
        completion_eval_autocompletion();
        return true;
      }
      ret = true;
    }
    ret = dwb_eval_key(e);
  }
  FREE(key);
  return ret;
}/*}}}*/

/* dwb_key_release_cb {{{*/
static gboolean 
dwb_key_release_cb(GtkWidget *w, GdkEventKey *e, View *v) {
  if (DWB_TAB_KEY(e)) {
    return true;
  }
  return false;
}/*}}}*/

/*}}}*/

/* COMMAND_TEXT {{{*/

/* dwb_set_status_bar_text(GList *gl, const char *text, GdkColor *fg,  PangoFontDescription *fd) {{{*/
void
dwb_set_status_bar_text(GtkWidget *label, const char *text, DwbColor *fg,  PangoFontDescription *fd, gboolean markup) {
  if (markup) {
    char *escaped =  g_markup_escape_text(text, -1);
    gtk_label_set_markup(GTK_LABEL(label), text);
    g_free(escaped);
  }
  else {
    gtk_label_set_text(GTK_LABEL(label), text);
  }
  if (fg) {
    DWB_WIDGET_OVERRIDE_COLOR(label, GTK_STATE_NORMAL, fg);
  }
  if (fd) {
    DWB_WIDGET_OVERRIDE_FONT(label, fd);
  }
}/*}}}*/

/* hide command text {{{*/
void 
dwb_source_remove(GList *gl) {
  guint id;
  View *v = gl->data;
  if ( (id = v->status->message_id) ) {
    g_source_remove(id);
    v->status->message_id = 0;
  }
}
static gpointer 
dwb_hide_message(GList *gl) {
  CLEAR_COMMAND_TEXT(gl);
  return NULL;
}/*}}}*/

/* dwb_set_normal_message {{{*/
void 
dwb_set_normal_message(GList *gl, gboolean hide, const char  *text, ...) {
  va_list arg_list; 
  View *v = gl->data;
  
  va_start(arg_list, text);
  char message[STRING_LENGTH];
  vsnprintf(message, STRING_LENGTH - 1, text, arg_list);
  va_end(arg_list);

  dwb_set_status_bar_text(v->lstatus, message, &dwb.color.active_fg, dwb.font.fd_active, false);

  dwb_source_remove(gl);
  if (hide) {
    v->status->message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, gl);
  }
}/*}}}*/

/* dwb_set_error_message {{{*/
void 
dwb_set_error_message(GList *gl, const char *error, ...) {
  va_list arg_list; 

  va_start(arg_list, error);
  char message[STRING_LENGTH];
  vsnprintf(message, STRING_LENGTH - 1, error, arg_list);
  va_end(arg_list);

  dwb_source_remove(gl);

  dwb_set_status_bar_text(VIEW(gl)->lstatus, message, &dwb.color.error, dwb.font.fd_active, false);
  VIEW(gl)->status->message_id = g_timeout_add_seconds(dwb.misc.message_delay, (GSourceFunc)dwb_hide_message, gl);
  gtk_widget_hide(dwb.gui.entry);
}/*}}}*/

void 
dwb_update_uri(GList *gl) {
  View *v = VIEW(gl);
  if (!gl)
    gl = dwb.state.fview;

  WebKitWebView *wv = WEBVIEW(gl);
  const char *uri = webkit_web_view_get_uri(wv);
  DwbColor *uricolor;
  if (v->status->ssl == SSL_TRUSTED) 
    uricolor = &dwb.color.ssl_trusted;
  else if (v->status->ssl == SSL_UNTRUSTED) 
    uricolor = &dwb.color.ssl_untrusted;
  else 
    uricolor = gl == dwb.state.fview ? &dwb.color.active_fg : &dwb.color.normal_fg;

  dwb_set_status_bar_text(v->urilabel, uri, uricolor, NULL, false);
}

/* dwb_update_status_text(GList *gl) {{{*/
void 
dwb_update_status_text(GList *gl, GtkAdjustment *a) {
  View *v = gl ? gl->data : dwb.state.fview->data;
    
  if (!a) {
    a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v->scroll));
  }
  dwb_update_uri(gl);
  GString *string = g_string_new(NULL);

  gboolean back = webkit_web_view_can_go_back(WEBKIT_WEB_VIEW(v->web));
  gboolean forward = webkit_web_view_can_go_forward(WEBKIT_WEB_VIEW(v->web));
  const char *bof = back && forward ? " [+-]" : back ? " [+]" : forward  ? " [-]" : " ";
  g_string_append(string, bof);

  if (a) {
    double lower = gtk_adjustment_get_lower(a);
    double upper = gtk_adjustment_get_upper(a) - gtk_adjustment_get_page_size(a) + lower;
    double value = gtk_adjustment_get_value(a); 
    char *position = 
      upper == lower ? g_strdup("[all]") : 
      value == lower ? g_strdup("[top]") : 
      value == upper ? g_strdup("[bot]") : 
      g_strdup_printf("[%02d%%]", (int)(value * 100/upper + 0.5));
    g_string_append(string, position);
    FREE(position);
  }
  if (v->status->scripts & SCRIPTS_BLOCKED) {
    const char *format = v->status->scripts & SCRIPTS_ALLOWED_TEMPORARY 
      ? "[<span foreground='%s'>S</span>]"
      : "[<span foreground='%s'><s>S</s></span>]";
    g_string_append_printf(string, format,  v->status->scripts & SCRIPTS_ALLOWED_TEMPORARY ? dwb.color.allow_color : dwb.color.block_color);
  }
  if ((v->status->pb_status & PLUGIN_STATUS_ENABLED) &&  (v->status->pb_status & PLUGIN_STATUS_HAS_PLUGIN)) {
    if (v->status->pb_status & PLUGIN_STATUS_DISCONNECTED) 
      g_string_append_printf(string, "[<span foreground='%s'>P</span>]",  dwb.color.allow_color);
    else 
      g_string_append_printf(string, "[<span foreground='%s'><s>P</s></span>]",  dwb.color.block_color);
  }
  if (v->status->progress != 0) {
    wchar_t buffer[PBAR_LENGTH + 1] = { 0 };
    wchar_t cbuffer[PBAR_LENGTH] = { 0 };
    int length = PBAR_LENGTH * v->status->progress / 100;
    wmemset(buffer, 0x2588, length - 1);
    buffer[length] = 0x258f - (int)((v->status->progress % 10) / 10.0*8);
    wmemset(cbuffer, 0x2581, PBAR_LENGTH-length-1);
    cbuffer[PBAR_LENGTH - length] = '\0';
    g_string_append_printf(string, "\u2595%ls%ls\u258f", buffer, cbuffer);
  }
  dwb_set_status_bar_text(v->rstatus, string->str, NULL, NULL, true);
  g_string_free(string, true);
}/*}}}*/

/*}}}*/

/* FUNCTIONS {{{*/

/* dwb_editor_watch (GChildWatchFunc) {{{*/
static void
dwb_editor_watch(GPid pid, int status, EditorInfo *info) {
  char *content = util_get_file_content(info->filename);
  WebKitDOMElement *e = NULL;
  WebKitWebView *wv;
  if (content == NULL) 
    goto clean;
  if (!info->gl || !g_list_find(dwb.state.views, info->gl->data)) {
    if (info->id == NULL) 
      goto clean;
    else 
      wv = CURRENT_WEBVIEW();
  }
  else 
    wv = WEBVIEW(info->gl);
  if (info->id != NULL) {
    WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
    e = webkit_dom_document_get_element_by_id(doc, info->id);
    if (e == NULL) 
      goto clean;
  }
  else 
    e = info->element;

  if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(e)) 
    webkit_dom_html_input_element_set_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), content);
  if (WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(e)) 
    webkit_dom_html_text_area_element_set_value(WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(e), content);

clean:
  unlink(info->filename);
  g_free(info->filename);
  FREE(info->id);
  g_free(info);
}/*}}}*/

/* dwb_open_in_editor(void) ret: gboolean success {{{*/
DwbStatus
dwb_open_in_editor(void) {
  DwbStatus ret = STATUS_OK;
  char *editor = GET_CHAR("editor");
  char **commands = NULL;
  char *commandstring = NULL;
  if (editor == NULL) 
    return STATUS_ERROR;
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(CURRENT_WEBVIEW());
  WebKitDOMElement *active = webkit_dom_html_document_get_active_element(WEBKIT_DOM_HTML_DOCUMENT(doc));
  if (active == NULL) 
    return STATUS_ERROR;

  char *tagname = webkit_dom_element_get_tag_name(active);
  if (tagname == NULL) 
    return STATUS_ERROR;
  char *value = NULL;
  if (! strcmp(tagname, "INPUT")) 
    value = webkit_dom_html_input_element_get_value(WEBKIT_DOM_HTML_INPUT_ELEMENT(active));
  else if (!strcmp(tagname, "TEXTAREA"))
    value = webkit_dom_html_text_area_element_get_value(WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT(active));
  if (value == NULL) 
    return STATUS_ERROR;

  char *path = util_get_temp_filename("edit");
  
  commandstring = util_string_replace(editor, "dwb_uri", path);
  if (commandstring == NULL)  {
    ret = STATUS_ERROR;
    goto clean;
  }

  g_file_set_contents(path, value, -1, NULL);
  commands = g_strsplit(commandstring, " ", -1);
  GPid pid;
  gboolean success = g_spawn_async(NULL, commands, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL);
  if (!success) {
    ret = STATUS_ERROR;
    goto clean;
  }

  EditorInfo *info = dwb_malloc(sizeof(EditorInfo));
  char *id = webkit_dom_html_element_get_id(WEBKIT_DOM_HTML_ELEMENT(active));
  if (id != NULL && strlen(id) > 0) {
    info->id = g_strdup(id);
  }
  else  {
    info->id = NULL;
  }
  info->tagname = tagname;
  info->element = active;
  info->filename = path;
  info->gl = dwb.state.fview;
  g_child_watch_add(pid, (GChildWatchFunc)dwb_editor_watch, info);

clean:
  FREE(commandstring);
  if (commands != NULL) 
    g_strfreev(commands);

  return ret;
}/*}}}*/

/* remove history, bookmark, quickmark {{{*/
static int
dwb_remove_navigation_item(GList **content, const char *line, const char *filename) {
  Navigation *n = dwb_navigation_new_from_line(line);
  GList *item = g_list_find_custom(*content, n, (GCompareFunc)util_navigation_compare_first);
  dwb_navigation_free(n);
  if (item) {
    if (filename != NULL) 
      util_file_remove_line(filename, line);
    *content = g_list_delete_link(*content, item);
    return 1;
  }
  return 0;
}
void
dwb_remove_bookmark(const char *line) {
  dwb_remove_navigation_item(&dwb.fc.bookmarks, line, dwb.files.bookmarks);
}
void
dwb_remove_history(const char *line) {
  dwb_remove_navigation_item(&dwb.fc.history, line, dwb.misc.synctimer <= 0 ? dwb.files.history : NULL);
}
void 
dwb_remove_quickmark(const char *line) {
  Quickmark *q = dwb_quickmark_new_from_line(line);
  GList *item = g_list_find_custom(dwb.fc.quickmarks, q, (GCompareFunc)util_quickmark_compare);
  dwb_quickmark_free(q);
  if (item) {
    util_file_remove_line(dwb.files.quickmarks, line);
    dwb.fc.quickmarks = g_list_delete_link(dwb.fc.quickmarks, item);
  }
#if 0
  char *line = g_strdup_printf("%s %s %s\n", q->key, q->nav->first, q->nav->second);

  g_free(line);
#endif
}/*}}}*/

/* dwb_sync_history {{{*/
static gboolean
dwb_sync_history(gpointer data) {
  GString *buffer = g_string_new(NULL);
  for (GList *gl = dwb.fc.history; gl; gl=gl->next) {
    Navigation *n = gl->data;
    g_string_append_printf(buffer, "%s %s\n", n->first, n->second);
  }
  g_file_set_contents(dwb.files.history, buffer->str, -1, NULL);
  g_string_free(buffer, true);
  return true;
}/*}}}*/

/* dwb_follow_selection() {{{*/
static void 
dwb_follow_selection() {
  char *href = NULL;
  WebKitDOMNode *n = NULL;
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(CURRENT_WEBVIEW());
  WebKitDOMDOMWindow *window = webkit_dom_document_get_default_view(doc);
  WebKitDOMDOMSelection *selection = webkit_dom_dom_window_get_selection(window);
  if (selection == NULL)  
    return;
  WebKitDOMRange *range = webkit_dom_dom_selection_get_range_at(selection, 0, NULL);
  if (range == NULL) 
    return;

  for (n = webkit_dom_range_get_start_container(range, NULL); 
      n && n != WEBKIT_DOM_NODE(webkit_dom_document_get_document_element(doc)) && href == NULL; 
      n = webkit_dom_node_get_parent_node(n)) 
  {
    if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT(n)) {
      href = webkit_dom_html_anchor_element_get_href(WEBKIT_DOM_HTML_ANCHOR_ELEMENT(n));
      webkit_web_view_load_uri(CURRENT_WEBVIEW(), href);
    }
  }
}/*}}}*/

/* dwb_open_startpage(GList *) {{{*/
DwbStatus
dwb_open_startpage(GList *gl) {
  if (!dwb.misc.startpage) 
    return STATUS_ERROR;
  if (gl == NULL) 
    gl = dwb.state.fview;

  Arg a = { .p = dwb.misc.startpage, .b = true };
  dwb_load_uri(gl, &a);
  return STATUS_OK;
}/*}}}*/

/* dwb_apply_settings(WebSettings *s) {{{*/
static void
dwb_apply_settings(WebSettings *s) {
  for (GList *l = dwb.state.views; l; l=l->next) 
    if (s->func) 
      s->func(l, s);
  dwb_change_mode(NORMAL_MODE, false);

}/*}}}*/

/* dwb_set_setting(const char *){{{*/
void
dwb_set_setting(const char *key, char *value) {
  WebSettings *s;
  Arg *a = NULL;

  GHashTable *t = dwb.settings;
  if (key) {
    if  ( (s = g_hash_table_lookup(t, key)) ) {
      if ( (a = util_char_to_arg(value, s->type)) || (s->type == CHAR && a->p == NULL)) {
        s->arg = *a;
        dwb_apply_settings(s);
        dwb_set_normal_message(dwb.state.fview, true, "Saved setting %s: %s", s->n.first, s->type == BOOLEAN ? ( s->arg.b ? "true" : "false") : value);
        dwb_save_settings();
      }
      else {
        dwb_set_error_message(dwb.state.fview, "No valid value.");
      }
    }
    else {
      dwb_set_error_message(dwb.state.fview, "No such setting: %s", key);
    }
  }
  dwb_change_mode(NORMAL_MODE, false);


}/*}}}*/

/* dwb_set_key(const char *prop, char *val) {{{*/
void
dwb_set_key(const char *prop, char *val) {
  KeyValue value;

  value.id = g_strdup(prop);
  if (val)
    value.key = dwb_str_to_key(val); 
  else {
    Key key = { NULL, 0 };
    value.key = key;
  }

  dwb_set_normal_message(dwb.state.fview, true, "Saved key for command %s: %s", prop, val ? val : "");

  dwb.keymap = dwb_keymap_add(dwb.keymap, value);
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)util_keymap_sort_second);

  dwb_change_mode(NORMAL_MODE, false);
}/*}}}*/

/* dwb_get_host(WebKitWebView *) {{{*/
char *
dwb_get_host(WebKitWebView *web) {
  char *host = NULL;
  SoupURI *uri = soup_uri_new(webkit_web_view_get_uri(web));
  if (uri) {
    host = g_strdup(uri->host);
    soup_uri_free(uri);
  }
  return host;

#if 0 
  /*  this sometimes segfaults */
  const char *host = NULL;
  WebKitSecurityOrigin *origin = webkit_web_frame_get_security_origin(webkit_web_view_get_main_frame(web));
  if (origin) {
    host = webkit_security_origin_get_host(origin);
  }
  return host;
#endif
}/*}}}*/

/* dwb_focus_view(GList *gl){{{*/
void
dwb_focus_view(GList *gl) {
  if (gl != dwb.state.fview) {
    if (dwb.state.layout & MAXIMIZED) { 
      if (gl == dwb.state.views) {
        gtk_widget_hide(dwb.gui.right);
        gtk_widget_show(dwb.gui.left);
      }
      else {
        gtk_widget_hide(dwb.gui.left);
        gtk_widget_show(dwb.gui.right);
      }
      gtk_widget_show(((View *)gl->data)->vbox);
      gtk_widget_hide(((View *)dwb.state.fview->data)->vbox);
    }
    dwb_unfocus();
    dwb_focus(gl);
  }
}/*}}}*/

/* dwb_get_allowed(const char *filename, const char *data) {{{*/
gboolean 
dwb_get_allowed(const char *filename, const char *data) {
  char *content;
  gboolean ret = false;
  g_file_get_contents(filename, &content, NULL, NULL);
  if (content) {
    char **lines = g_strsplit(content, "\n", -1);
    for (int i=0; i<g_strv_length(lines); i++) {
      if (!strcmp(lines[i], data)) {
        ret = true; 
        break;
      }
    }
    g_strfreev(lines);
    FREE(content);
  }
  return ret;
}/*}}}*/

/* dwb_toggle_allowed(const char *filename, const char *data) {{{*/
gboolean 
dwb_toggle_allowed(const char *filename, const char *data) {
  char *content = NULL;
  if (!data)
    return false;
  gboolean allowed = dwb_get_allowed(filename, data);
  g_file_get_contents(filename, &content, NULL, NULL);
  GString *buffer = g_string_new(NULL);
  if (!allowed) {
    if (content) {
      g_string_append(buffer, content);
    }
    g_string_append_printf(buffer, "%s\n", data);
  }
  else if (content) {
    char **lines = g_strsplit(content, "\n", -1);
    for (int i=0; i<g_strv_length(lines); i++) {
      if (strlen(lines[i]) && strcmp(lines[i], data)) {
        g_string_append_printf(buffer, "%s\n", lines[i]);
      }
    }
  }
  g_file_set_contents(filename, buffer->str, -1, NULL);

  FREE(content);
  g_string_free(buffer, true);

  return !allowed;
}/*}}}*/

/* dwb_history_back {{{*/
DwbStatus
dwb_history_back() {
  WebKitWebView *w = CURRENT_WEBVIEW();

  if (!webkit_web_view_can_go_back(w)) 
    return STATUS_ERROR;

  WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(w);

  if (bf_list == NULL) 
    return STATUS_ERROR;

  int n = MIN(webkit_web_back_forward_list_get_back_length(bf_list), NUMMOD);
  WebKitWebHistoryItem *item = webkit_web_back_forward_list_get_nth_item(bf_list, -n);
  char *uri = (char *)webkit_web_history_item_get_uri(item);
  if (g_str_has_prefix(uri, "file://")) {
    Arg a = { .p = uri, .b = false };
    webkit_web_back_forward_list_go_to_item(bf_list, item);
    dwb_load_uri(NULL, &a);
  }
  else {
    webkit_web_view_go_to_back_forward_item(w, item);
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_history_forward{{{*/
DwbStatus
dwb_history_forward() {
  WebKitWebView *w = CURRENT_WEBVIEW();

  if (!webkit_web_view_can_go_forward(w))
    return STATUS_ERROR;

  WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(w);

  if (bf_list == NULL) 
    return STATUS_ERROR;

  int n = MIN(webkit_web_back_forward_list_get_forward_length(bf_list), NUMMOD);
  WebKitWebHistoryItem *item = webkit_web_back_forward_list_get_nth_item(bf_list, n);
  char *uri = (char *)webkit_web_history_item_get_uri(item);
  if (g_str_has_prefix(uri, "file://")) {
    Arg a = { .p = uri, .b = false };
    webkit_web_back_forward_list_go_to_item(bf_list, item);
    dwb_load_uri(NULL, &a);
  }
  else {
    webkit_web_view_go_to_back_forward_item(w, item);
  }
  return STATUS_OK;
}/*}}}*/

/* dwb_block_ad (GList *, const char *uri)        return: gboolean{{{*/
gboolean 
dwb_block_ad(GList *gl, const char *uri) {
  if (!VIEW(gl)->status->adblocker) 
    return false;

  /* PRINT_DEBUG(uri); */
  for (GList *l = dwb.fc.adblock; l; l=l->next) {
    char *data = l->data;
    if (data != NULL) {
      if (data[0] == '@') {
        if (g_regex_match_simple(data + 1, uri, 0, 0) )
          return true;
      }
      else if (strstr(uri, data)) {
        return true;
      }
    }
  }
  return false;
}/*}}}*/

/* dwb_eval_tabbar_visible (const char *) {{{*/
static TabBarVisible 
dwb_eval_tabbar_visible(const char *arg) {
  if (!strcasecmp(arg, "never")) {
    return HIDE_TB_NEVER;
  }
  else if (!strcasecmp(arg, "always")) {
    return HIDE_TB_ALWAYS;
  }
  else if (!strcasecmp(arg, "tiled")) {
    return HIDE_TB_TILED;
  }
  return 0;
}/*}}}*/

/* dwb_toggle_tabbar() {{{*/
void
dwb_toggle_tabbar(void) {
  gboolean visible = gtk_widget_get_visible(dwb.gui.topbox);
  if (visible ) {
    if (dwb.state.tabbar_visible != HIDE_TB_NEVER && 
        (dwb.state.tabbar_visible == HIDE_TB_ALWAYS || (HIDE_TB_TILED && !(dwb.state.layout & MAXIMIZED)))) {
      gtk_widget_hide(dwb.gui.topbox);
    }
  }
  else if (dwb.state.tabbar_visible != HIDE_TB_ALWAYS && 
      (dwb.state.tabbar_visible == HIDE_TB_NEVER || (HIDE_TB_TILED && (dwb.state.layout & MAXIMIZED)))) {
      gtk_widget_show(dwb.gui.topbox);
  }
}/*}}}*/

/* dwb_eval_completion_type {{{*/
CompletionType 
dwb_eval_completion_type(void) {
  switch (CLEAN_MODE(dwb.state.mode)) {
    case SETTINGS_MODE:  return COMP_SETTINGS;
    case KEY_MODE:       return COMP_KEY;
    case COMMAND_MODE:   return COMP_COMMAND;
    case COMPLETE_BUFFER: return COMP_BUFFER;
    default:            return COMP_NONE;
  }
}/*}}}*/

/* dwb_clean_load_end(GList *) {{{*/
void 
dwb_clean_load_end(GList *gl) {
  View *v = gl->data;
  if (v->status->mimetype) {
    g_free(v->status->mimetype);
    v->status->mimetype = NULL;
  }
  if (dwb.state.mode == INSERT_MODE || dwb.state.mode == FIND_MODE) {  
    dwb_change_mode(NORMAL_MODE, true);
  }
}/*}}}*/

/* dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *)   return: (alloc) Navigation* {{{*/
/* TODO sqlite */
Navigation *
dwb_navigation_from_webkit_history_item(WebKitWebHistoryItem *item) {
  Navigation *n = NULL;
  const char *uri;
  const char *title;

  if (item) {
    uri = webkit_web_history_item_get_uri(item);
    title = webkit_web_history_item_get_title(item);
    n = dwb_navigation_new(uri, title);
  }
  return n;
}/*}}}*/

/* dwb_open_channel (const char *filename) {{{*/
static void dwb_open_channel(const char *filename) {
  dwb.misc.si_channel = g_io_channel_new_file(filename, "r+", NULL);
  g_io_add_watch(dwb.misc.si_channel, G_IO_IN, (GIOFunc)dwb_handle_channel, NULL);
}/*}}}*/

/* dwb_test_userscript (const char *)         return: char* (alloc) or NULL {{{*/
static char * 
dwb_test_userscript(const char *filename) {
  char *path = g_build_filename(dwb.files.userscripts, filename, NULL); 

  if (g_file_test(path, G_FILE_TEST_IS_REGULAR) || 
      (g_str_has_prefix(filename, dwb.files.userscripts) && g_file_test(filename, G_FILE_TEST_IS_REGULAR) && (path = g_strdup(filename))) ) {
    return path;
  }
  else {
    g_free(path);
  }
  return NULL;
}/*}}}*/

/* dwb_open_si_channel() {{{*/
static void
dwb_open_si_channel() {
  dwb_open_channel(dwb.files.unifile);
}/*}}}*/

/* dwb_focus(GList *gl) {{{*/
void 
dwb_unfocus() {
  if (dwb.state.fview) {
    view_set_normal_style(VIEW(dwb.state.fview));
    dwb_source_remove(dwb.state.fview);
    CLEAR_COMMAND_TEXT(dwb.state.fview);
    dwb.state.fview = NULL;
  }
} /*}}}*/

/* dwb_get_default_settings()         return: GHashTable {{{*/
GHashTable * 
dwb_get_default_settings() {
  GHashTable *ret = g_hash_table_new(g_str_hash, g_str_equal);
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    WebSettings *new = dwb_malloc(sizeof(WebSettings));
    *new = *s;
    char *value = g_strdup(s->n.first);
    g_hash_table_insert(ret, value, new);
  }
  return ret;
}/*}}}*/

/* dwb_entry_set_text(const char *) {{{*/
void 
dwb_entry_set_text(const char *text) {
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), text);
  gtk_editable_set_position(GTK_EDITABLE(dwb.gui.entry), -1);
}/*}}}*/

/* dwb_focus_entry() {{{*/
void 
dwb_focus_entry() {
  gtk_widget_show(dwb.gui.entry);
  gtk_widget_grab_focus(dwb.gui.entry);
  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
}/*}}}*/

/* dwb_focus_scroll (GList *){{{*/
void
dwb_focus_scroll(GList *gl) {
  if (gl == NULL)
    return;
  View *v = gl->data;
  gtk_widget_grab_focus(v->web);
  gtk_widget_hide(dwb.gui.entry);
}/*}}}*/

/* dwb_entry_position_word_back(int position)      return position{{{*/
int 
dwb_entry_position_word_back(int position) {
  char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  if (position == 0) {
    return position;
  }
  int old = position;
  while (IS_WORD_CHAR(text[position-1]) ) {
    --position;
  }
  while (isblank(text[position-1])) {
    --position;
  }
  if (old == position) {
    --position;
  }
  return position;
}/*}}}*/

/* dwb_com_entry_history_forward(int) {{{*/
int 
dwb_entry_position_word_forward(int position) {
  char *text = gtk_editable_get_chars(GTK_EDITABLE(dwb.gui.entry), 0, -1);

  int old = position;
  while ( IS_WORD_CHAR(text[++position]) );
  while ( isblank(text[++position]) );
  if (old == position) {
    position++;
  }
  return position;
}/*}}}*/

/* dwb_handle_mail(const char *uri)        return: true if it is a mail-address{{{*/
gboolean 
dwb_spawn(GList *gl, const char *prop, const char *uri) {
  const char *program;
  char *command;
  if ( (program = GET_CHAR(prop)) && (command = util_string_replace(program, "dwb_uri", uri)) ) {
    g_spawn_command_line_async(command, NULL);
    g_free(command);
    return true;
  }
  else {
    dwb_set_error_message(gl, "Cannot open %s", uri);
    return false;
  }
}/*}}}*/

/* dwb_resize(double size) {{{*/
void
dwb_resize(double size) {
  int fact = dwb.state.layout & BOTTOM_STACK;

  gtk_widget_set_size_request(dwb.gui.left,  (100 - size) * (fact^1),  (100 - size) *  fact);
  gtk_widget_set_size_request(dwb.gui.right, size * (fact^1), size * fact);
  dwb.state.size = size;
}/*}}}*/

/* dwb_reload_layout(GList *,  WebSettings  *s) {{{*/
static void 
dwb_reload_layout(GList *gl, WebSettings *s) {
  dwb_init_style();
  View *v;
  for (GList *l = dwb.state.views; l; l=l->next) {
    v = VIEW(l);
    if (l == dwb.state.fview) {
      view_set_active_style(v);
    }
    else {
      view_set_normal_style(v);
    }
    DWB_WIDGET_OVERRIDE_FONT(v->entry, dwb.font.fd_entry);
  }
}/*}}}*/

/* dwb_get_search_engine_uri(const char *uri) {{{*/
static char *
dwb_get_search_engine_uri(const char *uri, const char *text) {
  char *ret = NULL;
  if (uri) {
    char **token = g_strsplit(uri, HINT_SEARCH_SUBMIT, 2);
    ret = g_strconcat(token[0], text, token[1], NULL);
    g_strfreev(token);
  }
  return ret;
}/* }}} */

/* dwb_get_search_engine(const char *uri) {{{*/
char *
dwb_get_search_engine(const char *uri, gboolean force) {
  char *ret = NULL;
  if ( force || ((!strstr(uri, ".") || strstr(uri, " ")) && !strstr(uri, "localhost:"))) {
    char **token = g_strsplit(uri, " ", 2);
    for (GList *l = dwb.fc.searchengines; l; l=l->next) {
      Navigation *n = l->data;
      if (!strcmp(token[0], n->first)) {
        ret = dwb_get_search_engine_uri(n->second, token[1]);
        break;
      }
    }
    if (!ret) {
      ret = dwb_get_search_engine_uri(dwb.misc.default_search, uri);
    }
    g_strfreev(token);
  }
  return ret;
}/*}}}*/

/* dwb_submit_searchengine {{{*/
void 
dwb_submit_searchengine(void) {
  char *com = g_strdup_printf("DwbHintObj.submitSearchEngine(\"%s\")", HINT_SEARCH_SUBMIT);
  char *value;
  if ( (value = dwb_execute_script(MAIN_FRAME(), com, true))) {
    dwb.state.form_name = value;
  }
  FREE(com);
}/*}}}*/

/* dwb_save_searchengine {{{*/
void
dwb_save_searchengine(void) {
  char *text = g_strdup(GET_TEXT());
  dwb_change_mode(NORMAL_MODE, false);

  if (!text)
    return;

  g_strstrip(text);
  if (text && strlen(text) > 0) {
    dwb_append_navigation_with_argument(&dwb.fc.searchengines, text, dwb.state.search_engine);
    Navigation *n = g_list_last(dwb.fc.searchengines)->data;
    Navigation *cn = dwb_get_search_completion_from_navigation(dwb_navigation_dup(n));

    dwb.fc.se_completion = g_list_append(dwb.fc.se_completion, cn);
    util_file_add_navigation(dwb.files.searchengines, n, true, -1);

    dwb_set_normal_message(dwb.state.fview, true, "Searchengine saved");
    if (dwb.state.search_engine) {
      if (!dwb.misc.default_search) {
        dwb.misc.default_search = dwb.state.search_engine;
      }
      else  {
        g_free(dwb.state.search_engine);
        dwb.state.search_engine = NULL;
      }
    }
  }
  else {
    dwb_set_error_message(dwb.state.fview, "No keyword specified, aborting.");
  }
  g_free(text);
}/*}}}*/

/* dwb_layout_from_char {{{*/
static Layout
dwb_layout_from_char(const char *desc) {
  char **token = g_strsplit(desc, " ", 0);
  int i=0;
  Layout layout = 0;
  while (token[i]) {
    if (!(layout & BOTTOM_STACK) && !g_ascii_strcasecmp(token[i], "normal")) {
      layout |= NORMAL_LAYOUT;
    }
    else if (!(layout & NORMAL_LAYOUT) && !g_ascii_strcasecmp(token[i], "bottomstack")) {
      layout |= BOTTOM_STACK;
    }
    else if (!g_ascii_strcasecmp(token[i], "maximized")) {
      layout |= MAXIMIZED;
    }
    else {
      layout = NORMAL_LAYOUT;
    }
    i++;
  }
  g_strfreev(token);
  return layout;
}/*}}}*/

/* dwb_web_settings_get_value(const char *id, void *value) {{{*/
static Arg *  
dwb_web_settings_get_value(const char *id) {
  WebSettings *s = g_hash_table_lookup(dwb.settings, id);
  return &s->arg;
}/*}}}*/

/* update_hints {{{*/
gboolean
dwb_update_hints(GdkEventKey *e) {
  char *buffer = NULL;
  char *com = NULL;
  char input[BUFFER_LENGTH] = { 0 }, *val;
  gboolean ret = false;

  if (e->keyval == GDK_KEY_Return) {
    com = g_strdup("DwbHintObj.followActive()");
  }
  else if (DWB_TAB_KEY(e)) {
    if (e->state & GDK_SHIFT_MASK) {
      com = g_strdup("DwbHintObj.focusPrev()");
    }
    else {
      com = g_strdup("DwbHintObj.focusNext()");
    }
    ret = true;
  }
  else if (e->is_modifier) {
    return false;
  }
  else {
    val = util_keyval_to_char(e->keyval);
    snprintf(input, BUFFER_LENGTH - 1, "%s%s", GET_TEXT(), val ? val : "");
    com = g_strdup_printf("DwbHintObj.updateHints(\"%s\")", input);
    FREE(val);
  }
  if (com) {
    buffer = dwb_execute_script(MAIN_FRAME(), com, true);
    g_free(com);
  }
  if (buffer) { 
    if (!strcmp("_dwb_no_hints_", buffer)) {
      dwb_set_error_message(dwb.state.fview, NO_HINTS);
      dwb_change_mode(NORMAL_MODE, false);
    }
    else if (!strcmp(buffer, "_dwb_input_")) {
      dwb_change_mode(INSERT_MODE);
    }
    else if  (!strcmp(buffer, "_dwb_click_")) {
      dwb.state.scriptlock = 1;
      if (dwb.state.nv != OPEN_DOWNLOAD) {
        dwb_change_mode(NORMAL_MODE, true);
      }
    }
    else  if (!strcmp(buffer, "_dwb_check_")) {
      dwb_change_mode(NORMAL_MODE, true);
    }
    FREE(buffer);
  }
  return ret;
}/*}}}*/

/* dwb_execute_script {{{*/
char *
dwb_execute_script(WebKitWebFrame *frame, const char *com, gboolean ret) {
  JSValueRef eval_ret;
  size_t length;
  char *retval;

  JSContextRef context = webkit_web_frame_get_global_context(frame);
  JSStringRef text = JSStringCreateWithUTF8CString(com);
  eval_ret = JSEvaluateScript(context, text, JSContextGetGlobalObject(context), NULL, 0, NULL);
  JSStringRelease(text);

  if (eval_ret) {
    if (ret) {
      JSStringRef string = JSValueToStringCopy(context, eval_ret, NULL);
      length = JSStringGetMaximumUTF8CStringSize(string);
      retval = g_new(char, length+1);
      JSStringGetUTF8CString(string, retval, length);
      JSStringRelease(string);
      memset(retval+length, '\0', 1);
      return retval;
    }
  }
  return NULL;
}
/*}}}*/


/*prepend_navigation_with_argument(GList **fc, const char *first, const char *second) {{{*/
void
dwb_prepend_navigation_with_argument(GList **fc, const char *first, const char *second) {
  for (GList *l = (*fc); l; l=l->next) {
    Navigation *n = l->data;
    if (!strcmp(first, n->first)) {
      dwb_navigation_free(n);
      (*fc) = g_list_delete_link((*fc), l);
      break;
    }
  }
  Navigation *n = dwb_navigation_new(first, second);

  (*fc) = g_list_prepend((*fc), n);
}/*}}}*/

/*append_navigation_with_argument(GList **fc, const char *first, const char *second) {{{*/
void
dwb_append_navigation_with_argument(GList **fc, const char *first, const char *second) {
  for (GList *l = (*fc); l; l=l->next) {
    Navigation *n = l->data;
    if (!strcmp(first, n->first)) {
      dwb_navigation_free(n);
      (*fc) = g_list_delete_link((*fc), l);
      break;
    }
  }
  Navigation *n = dwb_navigation_new(first, second);

  (*fc) = g_list_append((*fc), n);
}/*}}}*/

/* dwb_prepend_navigation(GList *gl, GList *view) {{{*/
DwbStatus 
dwb_prepend_navigation(GList *gl, GList **fc) {
  WebKitWebView *w = WEBVIEW(gl);
  const char *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri) > 0) {
    const char *title = webkit_web_view_get_title(w);
    dwb_prepend_navigation_with_argument(fc, uri, title);
    return STATUS_OK;
  }
  return STATUS_ERROR;

}/*}}}*/

/* dwb_save_quickmark(const char *key) {{{*/
static void 
dwb_save_quickmark(const char *key) {
  WebKitWebView *w = WEBKIT_WEB_VIEW(((View*)dwb.state.fview->data)->web);
  const char *uri = webkit_web_view_get_uri(w);
  if (uri && strlen(uri)) {
    const char *title = webkit_web_view_get_title(w);
    for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
      Quickmark *q = l->data;
      if (!strcmp(key, q->key)) {
        dwb_quickmark_free(q);
        dwb.fc.quickmarks = g_list_delete_link(dwb.fc.quickmarks, l);
        break;
      }
    }
    dwb.fc.quickmarks = g_list_prepend(dwb.fc.quickmarks, dwb_quickmark_new(uri, title, key));
    char *text = g_strdup_printf("%s %s %s", key, uri, title);
    util_file_add(dwb.files.quickmarks, text, true, -1);
    g_free(text);
    dwb_change_mode(NORMAL_MODE, false);

    dwb_set_normal_message(dwb.state.fview, true, "Added quickmark: %s - %s", key, uri);
  }
  else {
    dwb_set_error_message(dwb.state.fview, NO_URL);
  }
}/*}}}*/

/* dwb_open_quickmark(const char *key){{{*/
static void 
dwb_open_quickmark(const char *key) {
  for (GList *l = dwb.fc.quickmarks; l; l=l->next) {
    Quickmark *q = l->data;
    if (!strcmp(key, q->key)) {
      Arg a = { .p = q->nav->first, .b = true };
      dwb_load_uri(NULL, &a);
      dwb_set_normal_message(dwb.state.fview, true, "Loading quickmark %s: %s", key, q->nav->first);
      dwb_change_mode(NORMAL_MODE, false);
      return;
    }
  }
  dwb_set_error_message(dwb.state.fview, "No such quickmark: %s", key);
}/*}}}*/

/* dwb_tab_label_set_text {{{*/
static void
dwb_tab_label_set_text(GList *gl, const char *text) {
  View *v = gl->data;
  const char *uri = text ? text : webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
  char *escaped = g_markup_printf_escaped("[<span foreground=\"%s\">%d</span>] %s", 
      dwb.color.tab_number_color, 
      g_list_position(dwb.state.views, gl), 
      uri ? uri : "about:blank");
  gtk_label_set_markup(GTK_LABEL(v->tablabel), escaped);

  g_free(escaped);
}/*}}}*/


/* dwb_update_status(GList *gl) {{{*/
void 
dwb_update_status(GList *gl) {
  View *v = gl->data;
  char *filename = NULL;
  WebKitWebView *w = WEBKIT_WEB_VIEW(v->web);
  const char *title = webkit_web_view_get_title(w);
  if (!title && v->status->mimetype && strcmp(v->status->mimetype, "text/html")) {
    const char *uri = webkit_web_view_get_uri(w);
    filename = g_path_get_basename(uri);
    title = filename;
  }
  if (!title) {
    title = dwb.misc.name;
  }

  if (gl == dwb.state.fview) {
    if (v->status->progress != 0) {
      char *text = g_strdup_printf("[%d%%] %s", v->status->progress, title);
      gtk_window_set_title(GTK_WINDOW(dwb.gui.window), text);
      g_free(text);
    }
    else {
      gtk_window_set_title(GTK_WINDOW(dwb.gui.window), title);
    }
  }
  dwb_tab_label_set_text(gl, title);

  dwb_update_status_text(gl, NULL);
  FREE(filename);
}/*}}}*/

/* dwb_focus(GList *gl) {{{*/
void 
dwb_focus(GList *gl) {
  View *v = gl->data;

  if (dwb.gui.entry) {
    gtk_widget_hide(dwb.gui.entry);
  }
  dwb.state.fview = gl;
  dwb.gui.entry = v->entry;
  view_set_active_style(VIEW(gl));
  dwb_focus_scroll(gl);
  dwb_update_status(gl);
}/*}}}*/

/* dwb_new_window(Arg *arg) {{{*/
void 
dwb_new_window(Arg *arg) {
  char *argv[6];

  argv[0] = (char *)dwb.misc.prog_path;
  argv[1] = "-p"; 
  argv[2] = (char *)dwb.misc.profile;
  argv[3] = "-n";
  argv[4] = arg->p;
  argv[5] = NULL;
  g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}/*}}}*/

/* dwb_check_directory(const char *) {{{*/
gboolean
dwb_check_directory(WebKitWebView *wv, const char *path, Arg *a, GError **error) {
  char *unescaped = g_uri_unescape_string(path, NULL);
  char *local = unescaped;
  gboolean ret = true;
  if (g_str_has_prefix(local, "file://")) 
    local += *(local + 8) == '/' ? 8 : 7;
  if (!g_file_test(local, G_FILE_TEST_IS_DIR)) {
    ret = false;
    goto out;
  }
  if (access(local, R_OK)) 
    g_set_error(error, 0, 1, "Cannot access %s", local);
  int len = strlen (local);
  if (len>1 && local[len-1] == '/')
    local[len-1] = '\0';

  dwb_show_directory(wv, local, a);
out:
  g_free(unescaped);
  return ret;
}/*}}}*/

gboolean
dwb_directory_toggle_hidden_cb(WebKitDOMElement *el, WebKitDOMEvent *ev, GList *gl) {
  dwb.state.hidden_files = webkit_dom_html_input_element_get_checked(WEBKIT_DOM_HTML_INPUT_ELEMENT(el));
  commands_reload(NULL, NULL);
  return true;
}
void
dwb_load_directory_cb(WebKitWebView *wv) {
  if (webkit_web_view_get_load_status(wv) != WEBKIT_LOAD_FINISHED) 
    return;
  WebKitDOMDocument *doc = webkit_web_view_get_dom_document(wv);
  WebKitDOMElement *e = webkit_dom_document_get_element_by_id(doc, "hidden_checkbox");
  webkit_dom_html_input_element_set_checked(WEBKIT_DOM_HTML_INPUT_ELEMENT(e), dwb.state.hidden_files);
  webkit_dom_event_target_add_event_listener(WEBKIT_DOM_EVENT_TARGET(e), "change", G_CALLBACK(dwb_directory_toggle_hidden_cb), false, dwb.state.fview);
  g_signal_handlers_disconnect_by_func(wv, dwb_load_directory_cb, NULL);
}

/* dwb_show_directory(WebKitWebView *, const char *path, Arg *arg) 
 * 
 * Param: 
 * WebKitWebView: 
 * path: 
 * Arg arg:  .p = fullpath
 * {{{*/
void 
dwb_show_directory(WebKitWebView *web, const char *path, const Arg *arg) {
  /* TODO needs fix: when opening local files close commandline  */
  char *fullpath; 
  const char *filename;
  char *newpath = NULL;
  const char *tmp;
  const char *orig_path;
  GSList *content = NULL;
  GDir *dir = NULL;
  GString *buffer;
  GError *error = NULL;
  if (g_path_is_absolute(path)) {
    orig_path = path;
  }
  else {
    /*  resolve absolute path */
    char *current_dir = g_get_current_dir();
    char *full = g_build_filename(current_dir, path, NULL);
    char **components = g_strsplit(full, "/", -1);

    g_free(current_dir);
    g_free(full);

    int up = 0;
    char *tmppath = NULL;
    for (int i = g_strv_length(components)-1; i>=0; i--) {
      if (!strcmp(components[i], "..")) 
        up++;
      else if (! strcmp(components[i], "."))
        continue;
      else if (up > 0) 
        up--;
      else {
        newpath = g_build_filename("/", components[i], tmppath, NULL); 
        if (tmppath != NULL)
          g_free(tmppath);
        tmppath = newpath;
      }
    }
    orig_path = newpath;
    g_strfreev(components);
  }

  if (! (dir = g_dir_open(orig_path, 0, &error))) {
    fprintf(stderr, "dwb error: %s\n", error->message);
    g_clear_error(&error);
    return;
  }

  fullpath = g_build_filename(orig_path, "..", NULL);
  content = g_slist_prepend(content, fullpath);
  while ( (filename = g_dir_read_name(dir)) ) {
    fullpath = g_build_filename(orig_path, filename, NULL);
    content = g_slist_prepend(content, fullpath);
  }
  content = g_slist_sort(content, (GCompareFunc)util_compare_path);
  g_dir_close(dir);
  buffer = g_string_new(NULL);
  for (GSList *l = content; l; l=l->next) {
    fullpath = l->data;
    filename = g_strrstr(fullpath, "/") + 1;
    if (!dwb.state.hidden_files && filename[0] == '.' && filename[1] != '.')
      continue;
    struct stat st;
    char time[100];
    char size[50];
    char class[25] = { 0 };
    char *link = NULL;
    char *printname = NULL;
    if (lstat(fullpath, &st) != 0) {
      fprintf(stderr, "stat failed for %s\n", fullpath);
      continue;
    }
    strftime(time, 99, "%x %X", localtime(&st.st_mtime));
    if (st.st_size > BPGB) 
      snprintf(size, 49, "%.1fG", (double)st.st_size / BPGB);
    else if (st.st_size > BPMB) 
      snprintf(size, 49, "%.1fM", (double)st.st_size / BPMB);
    else if (st.st_size > BPKB) 
      snprintf(size, 49, "%.1fK", (double)st.st_size / BPKB);
    else 
      snprintf(size, 49, "%lu", st.st_size);

    char perm[11];
    int bits = 0;
    if (S_ISREG(st.st_mode))
      perm[bits++] = '-';
    else if (S_ISCHR(st.st_mode)) {
      perm[bits++] = 'c';
      strcpy(class, "dwb_character_device");
    }
    else if (S_ISDIR(st.st_mode)) {
      perm[bits++] = 'd';
      strcpy(class, "dwb_directory");
    }
    else if (S_ISBLK(st.st_mode)) {
      perm[bits++] = 'b';
      strcpy(class, "dwb_blockdevice");
    }
    else if (S_ISFIFO(st.st_mode)) {
      perm[bits++] = 'f';
      strcpy(class, "dwb_fifo");
    }
    else if (S_ISLNK(st.st_mode)) {
      perm[bits++] = 'l';
      strcpy(class, "dwb_link");
      link = g_file_read_link(fullpath, NULL);
      if (link != NULL) {
        char *tmp_path = fullpath;
        printname = g_strdup_printf("%s -> %s", filename, link);
        l->data = fullpath = g_build_filename(orig_path, link, NULL);
        g_free(tmp_path);
        g_free(link);
      }
    }
    /* user permissions */
    perm[bits++] = st.st_mode & S_IRUSR ? 'r' : '-';
    perm[bits++] = st.st_mode & S_IWUSR ? 'w' : '-';
    if (st.st_mode & S_ISUID) {
      perm[bits++] = st.st_mode & S_IXUSR ? 's' : 'S';
      strcpy(class, "dwb_setuid");
    }
    else
      perm[bits++] = st.st_mode & S_IXUSR ? 'x' : '-';
    if (st.st_mode & S_IXUSR && *class == 0) 
      strcpy(class, "dwb_executable");
    /*  group permissons */
    perm[bits++] = st.st_mode & S_IRGRP ? 'r' : '-';
    perm[bits++] = st.st_mode & S_IWGRP ? 'w' : '-';
    if (st.st_mode & S_ISGID) {
      perm[bits++] = st.st_mode & S_IXGRP ? 's' : 'S';
      strcpy(class, "dwb_setuid");
    }
    else
      perm[bits++] = st.st_mode & S_IXGRP ? 'x' : '-';
    /*  other */
    perm[bits++] = st.st_mode & S_IRGRP ? 'r' : '-';
    perm[bits++] = st.st_mode & S_IWGRP ? 'w' : '-';
    if (st.st_mode & S_ISVTX) {
      perm[bits++] = st.st_mode & S_IXGRP ? 't' : 'T';
      strcpy(class, "dwb_sticky");
    }
    else
      perm[bits++] = st.st_mode & S_IXGRP ? 'x' : '-';
    perm[bits] = '\0';
    struct passwd *pwd = getpwuid(st.st_uid);
    char *user = pwd && pwd->pw_name ? pwd->pw_name : "";
    struct group *grp = getgrgid(st.st_gid);
    char *group = pwd && grp->gr_name ? grp->gr_name : "";
    if (*class == 0)
      strcpy(class, "dwb_regular");

    g_string_append_printf(buffer, "<div class='tableRow'><div>%s</div><div>%lu</div><div>%s</div><div>%s</div>", perm, st.st_nlink, user, group);
    g_string_append_printf(buffer, "<div>%s</div>", size);
    g_string_append_printf(buffer, "<div>%s</div>", time);
    g_string_append_printf(buffer, "<div class='%s'><a href='%s'>%s</a></div></div>", class, fullpath, printname == NULL ? filename: printname);
    FREE(printname);

  }
  tmp = orig_path+1;
  char *match;
  GString *path_buffer = g_string_new("/<a class='headLine' href='");
  while ((match = strchr(tmp, '/'))) {
    g_string_append_len(path_buffer, orig_path, match-orig_path);
    g_string_append(path_buffer, "'>");
    g_string_append_len(path_buffer, tmp, match-tmp);
    tmp = match+1;
    g_string_append(path_buffer, "</a>/<a href='");
  }
  g_string_append_len(path_buffer, orig_path, match-orig_path);
  g_string_append(path_buffer, "'>");
  g_string_append_len(path_buffer, tmp, match-tmp);
  g_string_append(path_buffer, "</a>");

  char *local_file = util_get_data_file(LOCAL_FILE);
  char *filecontent = util_get_file_content(local_file);
  char *favicon = dwb_get_stock_item_base64_encoded("gtk-harddisk");
  /*  title, favicon, toppath, content */
  char *page = g_strdup_printf(filecontent, orig_path, favicon, path_buffer->str, buffer->str);

  g_free(favicon);
  g_free(local_file);
  g_free(filecontent);
  g_string_free(buffer, true);
  g_string_free(path_buffer, true);

  fullpath = g_str_has_prefix(arg->p, "file://") ? g_strdup(arg->p) : g_strdup_printf("file:///%s", orig_path);
  /* add a history item */
  /* TODO sqlite */
  if (arg->b) {
    WebKitWebBackForwardList *bf_list = webkit_web_view_get_back_forward_list(web);
    WebKitWebHistoryItem *item = webkit_web_history_item_new_with_data(fullpath, fullpath);
    webkit_web_back_forward_list_add_item(bf_list, item);
  }

  g_signal_connect(web, "notify::load-status", G_CALLBACK(dwb_load_directory_cb), NULL);
  webkit_web_view_load_string(web, page, NULL, NULL, fullpath);

  FREE(fullpath);
  g_free(page);
  FREE(newpath);

  for (GSList *l = content; l; l=l->next) {
    g_free(l->data);
  }
  g_slist_free(content);
}/*}}}*/

/* dwb_load_uri(const char *uri) {{{*/
void 
dwb_load_uri(GList *gl, Arg *arg) {
  /* TODO parse scheme */
  if (arg->p != NULL && strlen(arg->p) > 0)
    g_strstrip(arg->p);
  WebKitWebView *web = gl ? WEBVIEW(gl) : CURRENT_WEBVIEW();

  if (!arg || !arg->p || !strlen(arg->p)) {
    return;
  }

  /* new window ? */
  if (dwb.state.nv == OPEN_NEW_WINDOW) {
    dwb.state.nv = OPEN_NORMAL;
    dwb_new_window(arg);
    return;
  }
  /*  new tab ?  */
  else if (dwb.state.nv == OPEN_NEW_VIEW) {
    dwb.state.nv = OPEN_NORMAL;
    view_add(arg, false);
    return;
  }
  /*  get resolved uri */
  char *tmp, *uri = NULL; 
  GError *error = NULL;

  /* free cookie of last visited website */
  if (dwb.state.last_cookies) {
    soup_cookies_free(dwb.state.last_cookies); 
    dwb.state.last_cookies = NULL;
  }

  g_strstrip(arg->p);

  /* Check if uri is a html-string */
  if (dwb.state.type == HTML_STRING) {
    webkit_web_view_load_string(web, arg->p, "text/html", NULL, NULL);
    dwb.state.type = 0;
    return;
  }
  /* Check if uri is a userscript */
  if ( (uri = dwb_test_userscript(arg->p)) ) {
    Arg a = { .arg = uri };
    dwb_execute_user_script(NULL, &a);
    g_free(uri);
    return;
  }
  /* Check if uri is a javascript snippet */
  if (g_str_has_prefix(arg->p, "javascript:")) {
    dwb_execute_script(webkit_web_view_get_main_frame(web), arg->p, false);
    return;
  }
  /* Check if uri is a directory */
  if ( (dwb_check_directory(web, arg->p, arg, NULL)) ) {
    return;
  }
  /* Check if uri is a regular file */
  if (g_str_has_prefix(arg->p, "file://") || !strcmp(arg->p, "about:blank")) {
    webkit_web_view_load_uri(web, arg->p);
    return;
  }
  else if ( g_file_test(arg->p, G_FILE_TEST_IS_REGULAR) ) {
    if ( !(uri = g_filename_to_uri(arg->p, NULL, &error)) ) { 
      if (error->code == G_CONVERT_ERROR_NOT_ABSOLUTE_PATH) {
        g_clear_error(&error);
        char *path = g_get_current_dir();
        tmp = g_build_filename(path, arg->p, NULL);
        if ( !(uri = g_filename_to_uri(tmp, NULL, &error))) {
          fprintf(stderr, "Cannot open %s: %s", (char*)arg->p, error->message);
          g_clear_error(&error);
        }
        FREE(tmp);
        FREE(path);
      }
    }
  }
  else if (g_str_has_prefix(arg->p, "dwb://")) {
    webkit_web_view_load_uri(web, arg->p);
    return;
  }
  /* Check if searchengine is needed and load uri */

  else if (!(uri = dwb_get_search_engine(arg->p, false)) || strstr(arg->p, "localhost:")) {
    uri = g_str_has_prefix(arg->p, "http://") || g_str_has_prefix(arg->p, "https://") 
      ? g_strdup(arg->p)
      : g_strdup_printf("http://%s", (char*)arg->p);
  }
  webkit_web_view_load_uri(web, uri);
  FREE(uri);
}/*}}}*/

/* dwb_update_layout() {{{*/
void
dwb_update_layout(gboolean background) {
  gboolean visible = gtk_widget_get_visible(dwb.gui.right);
  WebKitWebView *w;

  if (! (dwb.state.layout & MAXIMIZED)) {
    if (dwb.state.views->next) {
      if (!visible) {
        gtk_widget_show_all(dwb.gui.right);
        gtk_widget_hide(((View*)dwb.state.views->next->data)->entry);
      }
      w = WEBVIEW(dwb.state.views->next);
      if (dwb.misc.factor != 1.0) {
        webkit_web_view_set_zoom_level(w, dwb.misc.factor);
      }
      Arg *a = dwb_web_settings_get_value("full-content-zoom");
      webkit_web_view_set_full_content_zoom(w, a->b);
    }
    else if (visible) {
      gtk_widget_hide(dwb.gui.right);
    }

    w = WEBVIEW(dwb.state.views);
    webkit_web_view_set_zoom_level(w, 1.0);
    dwb_resize(dwb.state.size);
  }
  /* update tab label */
  for (GList *gl = dwb.state.views; gl; gl = gl->next) {
    View *v = gl->data;
    const char *title = webkit_web_view_get_title(WEBKIT_WEB_VIEW(v->web));
    dwb_tab_label_set_text(gl, title);
  }
}/*}}}*/

/* dwb_eval_editing_key(GdkEventKey *) {{{*/
gboolean 
dwb_eval_editing_key(GdkEventKey *e) {
  if (!(ALPHA(e) || DIGIT(e))) {
    return false;
  }

  char *key = util_keyval_to_char(e->keyval);
  gboolean ret = false;

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (km->map->entry) {
      if (!strcmp(key, km->key) && CLEAN_STATE(e) == km->mod) {
        km->map->func(&km, &km->map->arg);
        ret = true;
        break;
      }
    }
  }
  FREE(key);
  return ret;
}/*}}}*/

/* dwb_clean_key_buffer() {{{*/
void 
dwb_clean_key_buffer() {
  dwb.state.nummod = 0;
  if (dwb.state.buffer) {
    g_string_free(dwb.state.buffer, true);
    dwb.state.buffer = NULL;
  }
}/*}}}*/

/* dwb_eval_key(GdkEventKey *e) {{{*/
static gboolean
dwb_eval_key(GdkEventKey *e) {
  gboolean ret = false;
  const char *old = dwb.state.buffer ? dwb.state.buffer->str : NULL;
  int keyval = e->keyval;
  unsigned int mod_mask;
  int keynum = -1;

  if (dwb.state.scriptlock) {
    return true;
  }
  if (e->is_modifier) {
    return false;
  }
  /* don't show backspace in the buffer */
  if (keyval == GDK_KEY_BackSpace ) {
    if (dwb.state.mode & AUTO_COMPLETE) {
      completion_clean_autocompletion();
    }
    if (dwb.state.buffer && dwb.state.buffer->str ) {
      if (dwb.state.buffer->len) {
        g_string_erase(dwb.state.buffer, dwb.state.buffer->len - 1, 1);
        dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_active, false);
      }
      ret = false;
    }
    else {
      ret = true;
    }
    return ret;
  }
  /* Multimedia keys */
  switch (keyval) {
    case GDK_KEY_Back : dwb_history_back(); return true;
    case GDK_KEY_Forward : dwb_history_forward(); return true;
    case GDK_KEY_Cancel : commands_stop_loading(NULL, NULL); return true;
    case GDK_KEY_Reload : commands_reload(NULL, NULL); return true;
    case GDK_KEY_ZoomIn : commands_zoom_in(NULL, NULL); return true;
    case GDK_KEY_ZoomOut : commands_zoom_out(NULL, NULL); return true;
  }
  char *key = util_keyval_to_char(keyval);
  if (key) {
    mod_mask = CLEAN_STATE(e);
  }
  else if ( (key = g_strdup(gdk_keyval_name(keyval)))) {
    mod_mask = CLEAN_STATE_WITH_SHIFT(e);
  }
  else {
    return false;
  }
  if (!old) {
    dwb.state.buffer = g_string_new(NULL);
    old = dwb.state.buffer->str;
  }
  /* nummod */
  if (DIGIT(e)) {
    keynum = e->keyval - GDK_KEY_0;
    if (dwb.state.nummod) {
      dwb.state.nummod = MIN(10*dwb.state.nummod + keynum, 314159);
    }
    else {
      dwb.state.nummod = e->keyval - GDK_KEY_0;
    }
    if (mod_mask) {
      for (GList *l = dwb.keymap; l; l=l->next) {
        KeyMap *km = l->data;
        if ((km->mod & DWB_NUMMOD_MASK) && (km->mod & ~DWB_NUMMOD_MASK) == mod_mask) {
          commands_simple_command(km);
          break;
        }
      }
    }
    PRINT_DEBUG("nummod: %d", dwb.state.nummod);
    FREE(key);
    return false;
  }
  g_string_append(dwb.state.buffer, key);
  if (ALPHA(e) || DIGIT(e)) {
    dwb_set_status_bar_text(VIEW(dwb.state.fview)->lstatus, dwb.state.buffer->str, &dwb.color.active_fg, dwb.font.fd_active, false);
  }

  const char *buf = dwb.state.buffer->str;
  int longest = 0;
  KeyMap *tmp = NULL;
  GList *coms = NULL;

  PRINT_DEBUG("buffer: %s key: %s", buf, key);
  PRINT_DEBUG("%d", e->state);

  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *km = l->data;
    if (km->map->entry) {
      continue;
    }
    gsize kl = strlen(km->key);
    if (!km->key || !kl ) {
      continue;
    }
    if (g_str_has_prefix(km->key, buf) && (mod_mask == km->mod) ) {
      if  (!longest || kl > longest) {
        longest = kl;
        tmp = km;
      }
      if (dwb.comps.autocompletion) {
        coms = g_list_append(coms, km);
      }
      ret = true;
    }
  }
  /* autocompletion */
  if (dwb.state.mode & AUTO_COMPLETE) {
    completion_clean_autocompletion();
  }
  if (coms && g_list_length(coms) > 0) {
    completion_autocomplete(coms, NULL);
    ret = true;
  }
  if (tmp && dwb.state.buffer->len == longest) {
    commands_simple_command(tmp);
  }
  if (longest == 0) {
    dwb_clean_key_buffer();
    CLEAR_COMMAND_TEXT(dwb.state.fview);
  }
  FREE(key);
  return ret;

}/*}}}*/

/* dwb_insert_mode(Arg *arg) {{{*/
static DwbStatus
dwb_insert_mode(void) {
  if (dwb.state.mode & PASS_THROUGH)
    return STATUS_ERROR;
  if (dwb.state.mode == HINT_MODE) {
    dwb_set_normal_message(dwb.state.fview, true, INSERT);
  }
  //view_modify_style(CURRENT_VIEW(), &dwb.color.insert_fg, &dwb.color.insert_bg, NULL, NULL, NULL);
  dwb_set_normal_message(dwb.state.fview, false, "-- INSERT MODE --");

  dwb.state.mode = INSERT_MODE;
  return STATUS_OK;
}/*}}}*/

/* dwb_command_mode(void) {{{*/
static DwbStatus 
dwb_command_mode(void) {
  dwb_set_normal_message(dwb.state.fview, false, ":");
  dwb_focus_entry();
  dwb.state.mode = COMMAND_MODE;
  return STATUS_OK;
}/*}}}*/

static DwbStatus
dwb_passthrough_mode(void) {
  dwb.state.mode |= PASS_THROUGH;
  dwb_set_normal_message(dwb.state.fview, false, "-- PASS THROUGH --");
  return STATUS_OK;
}

/* dwb_normal_mode() {{{*/
static DwbStatus 
dwb_normal_mode(gboolean clean) {
  Mode mode = dwb.state.mode;

  if (mode == HINT_MODE || mode == SEARCH_FIELD_MODE) {
    dwb_execute_script(MAIN_FRAME(), "DwbHintObj.clear()", false);
  }
  //else if (mode & INSERT_MODE) {
  //  view_modify_style(CURRENT_VIEW(), &dwb.color.active_fg, &dwb.color.active_bg, NULL, NULL, NULL);
  //  gtk_entry_set_visibility(GTK_ENTRY(dwb.gui.entry), true);
  //}
  else if (mode == DOWNLOAD_GET_PATH) {
    completion_clean_path_completion();
  }
  if (mode & COMPLETION_MODE) {
    completion_clean_completion();
  }
  if (mode & AUTO_COMPLETE) {
    completion_clean_autocompletion();
  }
  dwb_focus_scroll(dwb.state.fview);

  if (clean) {
    if (dwb.state.buffer != NULL) {
      g_string_free(dwb.state.buffer, true);
      dwb.state.buffer = NULL;
    }
    CLEAR_COMMAND_TEXT(dwb.state.fview);
  }

  gtk_entry_set_text(GTK_ENTRY(dwb.gui.entry), "");
  dwb_clean_vars();
  return STATUS_OK;
}/*}}}*/

DwbStatus 
dwb_change_mode(Mode mode, ...) {
  GStaticMutex mutex = G_STATIC_MUTEX_INIT;
  DwbStatus ret = STATUS_OK;
  gboolean clean;
  va_list vl;
  if (! g_static_mutex_trylock(&mutex))
    return STATUS_ERROR;

  switch(mode) {
    case NORMAL_MODE: 
      va_start(vl, mode);
      clean = va_arg(vl, gboolean);
      ret = dwb_normal_mode(clean);
      va_end(vl);
      break;
    case INSERT_MODE:   ret = dwb_insert_mode(); break;
    case COMMAND_MODE:  ret = dwb_command_mode(); break;
    case PASS_THROUGH:  ret = dwb_passthrough_mode(); break;
    default: PRINT_DEBUG("Unknown mode: %d", mode); break;
  }
  g_static_mutex_unlock(&mutex);
  return ret;
}


/* gboolean dwb_highlight_search(void) {{{*/
gboolean
dwb_highlight_search() {
  View *v = CURRENT_VIEW();
  WebKitWebView *web = WEBKIT_WEB_VIEW(v->web);
  int matches;
  webkit_web_view_unmark_text_matches(web);

  if ( v->status->search_string != NULL && (matches = webkit_web_view_mark_text_matches(web, v->status->search_string, false, 0)) ) {
    dwb_set_normal_message(dwb.state.fview, false, "[%3d hits] ", matches);
    webkit_web_view_set_highlight_text_matches(web, true);
    return true;
  }
  return false;
}/*}}}*/

/* dwb_update_search(gboolean ) {{{*/
gboolean 
dwb_update_search(gboolean forward) {
  View *v = CURRENT_VIEW();
  const char *text = GET_TEXT();
  if (strlen(text) > 0) {
    FREE(v->status->search_string);
    v->status->search_string =  g_strdup(text);
  }
  if (!v->status->search_string) {
    return false;
  }
  if (! dwb_highlight_search()) {
    dwb_set_status_bar_text(CURRENT_VIEW()->lstatus, "[  0 hits] ", &dwb.color.error, dwb.font.fd_active, false);
    return false;
  }
  return true;
}/*}}}*/

/* dwb_search {{{*/
gboolean
dwb_search(KeyMap *km, Arg *arg) {
  View *v = CURRENT_VIEW();
  gboolean forward = dwb.state.forward_search;
  if (arg) {
    if (!arg->b) {
      forward = !dwb.state.forward_search;
    }
    dwb_highlight_search();
  }
  if (v->status->search_string) {
    webkit_web_view_search_text(WEBKIT_WEB_VIEW(v->web), v->status->search_string, false, forward, true);
  }
  return true;
}/*}}}*/

/* dwb_user_script_cb(GIOChannel *, GIOCondition *)     return: false {{{*/
static gboolean
dwb_user_script_cb(GIOChannel *channel, GIOCondition condition, GIOChannel *out_channel) {
  GError *error = NULL;
  char *line;

  while (g_io_channel_read_line(channel, &line, NULL, NULL, &error) == G_IO_STATUS_NORMAL) {
    if (g_str_has_prefix(line, "javascript:")) {
      char *value; 
      if ( ( value = dwb_execute_script(MAIN_FRAME(), line + 11, true))) {
        g_io_channel_write_chars(out_channel, value, -1, NULL, NULL);
        g_io_channel_write_chars(out_channel, "\n", 1, NULL, NULL);
        g_free(value);
      }
    }
    else if (!strcmp(line, "close\n")) {
      g_io_channel_shutdown(channel, true, NULL);
      FREE(line);
      break;
    }
    else {
      dwb_parse_command_line(g_strchomp(line));
    }
    g_io_channel_flush(out_channel, NULL);
    FREE(line);
  }
  if (error) {
    fprintf(stderr, "Cannot read from std_out: %s\n", error->message);
  }
  g_clear_error(&error);

  return false;
}/*}}}*/

/* dwb_execute_user_script(Arg *a) {{{*/
void
dwb_execute_user_script(KeyMap *km, Arg *a) {
  GError *error = NULL;
  char nummod[64];
  snprintf(nummod, 64, "%d", NUMMOD);
  char *argv[6] = { a->arg, (char*)webkit_web_view_get_uri(CURRENT_WEBVIEW()), (char *)dwb.misc.profile, nummod, a->p, NULL } ;
  int std_out;
  int std_in;
  if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &std_in, &std_out, NULL, &error)) {
    GIOChannel *channel = g_io_channel_unix_new(std_out);
    GIOChannel *out_channel = g_io_channel_unix_new(std_in);
    g_io_add_watch(channel, G_IO_IN, (GIOFunc)dwb_user_script_cb, out_channel);
    dwb_set_normal_message(dwb.state.fview, true, "Executing script %s", a->arg);
  }
  else {
    fprintf(stderr, "Cannot execute %s: %s\n", (char*)a->p, error->message);
  }
  g_clear_error(&error);
}/*}}}*/

/* dwb_get_scripts() {{{*/
static GList * 
dwb_get_scripts() {
  GDir *dir;
  char *filename;
  char *content;
  GList *gl = NULL;
  Navigation *n = NULL;

  if ( (dir = g_dir_open(dwb.files.userscripts, 0, NULL)) ) {
    while ( (filename = (char*)g_dir_read_name(dir)) ) {
      char *path = g_build_filename(dwb.files.userscripts, filename, NULL);

      g_file_get_contents(path, &content, NULL, NULL);
      char **lines = g_strsplit(content, "\n", -1);
      g_free(content);
      int i=0;
      KeyMap *map = dwb_malloc(sizeof(KeyMap));
      FunctionMap *fmap = dwb_malloc(sizeof(FunctionMap));
      while (lines[i]) {
        if (g_regex_match_simple(".*dwb:", lines[i], 0, 0)) {
          char **line = g_strsplit(lines[i], "dwb:", 2);
          if (line[1]) {
            n = dwb_navigation_new(filename, line[1]);
            Key key = dwb_str_to_key(line[1]);
            map->key = key.str;
            map->mod = key.mod;
            gl = g_list_prepend(gl, map);
          }
          g_strfreev(line);
          break;
        }
        i++;
      }
      if (!n) {
        n = dwb_navigation_new(filename, "");
        map->key = "";
        map->mod = 0;
        gl = g_list_prepend(gl, map);
      }
      FunctionMap fm = { { n->first, n->first }, CP_DONT_SAVE | CP_COMMANDLINE, (Func)dwb_execute_user_script, NULL, POST_SM, { .arg = path } };
      *fmap = fm;
      map->map = fmap;
      dwb.misc.userscripts = g_list_prepend(dwb.misc.userscripts, n);
      n = NULL;

      g_strfreev(lines);
    }
    g_dir_close(dir);
  }
  return gl;
}/*}}}*/

/*}}}*/

/* EXIT {{{*/

/* dwb_clean_vars() {{{*/
static void 
dwb_clean_vars() {
  dwb.state.mode = NORMAL_MODE;
  dwb.state.buffer = NULL;
  dwb.state.nummod = 0;
  dwb.state.nv = OPEN_NORMAL;
  dwb.state.type = 0;
  dwb.state.scriptlock = 0;
  dwb.state.last_com_history = NULL;
  dwb.state.dl_action = DL_ACTION_DOWNLOAD;
}/*}}}*/

static void
dwb_free_list(GList *list, void (*func)(void*)) {
  for (GList *l = list; l; l=l->next) {
    Navigation *n = l->data;
    func(n);
  }
  g_list_free(list);
}

/* dwb_clean_up() {{{*/
gboolean
dwb_clean_up() {
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *m = l->data;
    FREE(m);
  }
  g_list_free(dwb.keymap);
  g_hash_table_remove_all(dwb.settings);

  dwb_free_list(dwb.fc.bookmarks, (void_func)dwb_navigation_free);
  /*  TODO sqlite */
  dwb_free_list(dwb.fc.history, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.searchengines, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.se_completion, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.mimetypes, (void_func)dwb_navigation_free);
  dwb_free_list(dwb.fc.quickmarks, (void_func)dwb_quickmark_free);
  dwb_free_list(dwb.fc.cookies_allow, (void_func)dwb_free);
  dwb_free_list(dwb.fc.adblock, (void_func)dwb_free);


  if (g_file_test(dwb.files.fifo, G_FILE_TEST_EXISTS)) {
    unlink(dwb.files.fifo);
  }
  gtk_widget_destroy(dwb.gui.window);
  return true;
}/*}}}*/

/* dwb_save_keys() {{{*/
static void
dwb_save_keys() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  char *content;
  gsize size;

  if (!g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No keysfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  for (GList *l = dwb.keymap; l; l=l->next) {
    KeyMap *map = l->data;
    if (! (map->map->prop & CP_DONT_SAVE) ) {
      char *mod = dwb_modmask_to_string(map->mod);
      char *sc = g_strdup_printf("%s %s", mod, map->key ? map->key : "");
      g_key_file_set_value(keyfile, dwb.misc.profile, map->map->n.first, sc);
      FREE(sc);
      FREE(mod);
    }
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    g_file_set_contents(dwb.files.keys, content, size, &error);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save keyfile: %s", error->message);
    g_clear_error(&error);
  }
  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_save_settings {{{*/
void
dwb_save_settings() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  char *content;
  gsize size;
  setlocale(LC_NUMERIC, "C");

  if (!g_key_file_load_from_file(keyfile, dwb.files.settings, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    fprintf(stderr, "No settingsfile found, creating a new file.\n");
    g_clear_error(&error);
  }
  for (GList *l = g_hash_table_get_values(dwb.settings); l; l=l->next) {
    WebSettings *s = l->data;
    char *value = util_arg_to_char(&s->arg, s->type); 
    g_key_file_set_value(keyfile, dwb.misc.profile, s->n.first, value ? value : "" );

    FREE(value);
  }
  if ( (content = g_key_file_to_data(keyfile, &size, &error)) ) {
    g_file_set_contents(dwb.files.settings, content, size, &error);
    g_free(content);
  }
  if (error) {
    fprintf(stderr, "Couldn't save settingsfile: %s\n", error->message);
    g_clear_error(&error);
  }
  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_save_files() {{{*/
gboolean 
dwb_save_files(gboolean end_session) {
  dwb_save_keys();
  dwb_save_settings();
  if (dwb.misc.synctimer > 0) {
    dwb_sync_history(NULL);
  }

  /* save session */
  if (end_session && GET_BOOL("save-session") && dwb.state.mode != SAVE_SESSION) {
    session_save(NULL);
  }
  return true;
}
/* }}} */

/* dwb_end() {{{*/
gboolean
dwb_end() {
  if (dwb_save_files(true)) {
    if (dwb_clean_up()) {
      gtk_main_quit();
      return true;
    }
  }
  return false;
}/*}}}*/
/* }}} */

/* KEYS {{{*/

/* dwb_str_to_key(char *str)      return: Key{{{*/
Key 
dwb_str_to_key(char *str) {
  Key key = { .mod = 0, .str = NULL };
  GString *buffer = g_string_new(NULL);

  char **string = g_strsplit(str, " ", -1);

  for (int i=0; i<g_strv_length(string); i++)  {
    if (!g_ascii_strcasecmp(string[i], "Control")) {
      key.mod |= GDK_CONTROL_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod1")) {
      key.mod |= GDK_MOD1_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Mod4")) {
      key.mod |= GDK_MOD4_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button1")) {
      key.mod |= GDK_BUTTON1_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button2")) {
      key.mod |= GDK_BUTTON2_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button3")) {
      key.mod |= GDK_BUTTON3_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button4")) {
      key.mod |= GDK_BUTTON4_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Button5")) {
      key.mod |= GDK_BUTTON5_MASK;
    }
    else if (!g_ascii_strcasecmp(string[i], "Shift")) {
      key.mod |= GDK_SHIFT_MASK;
    }
    else if (!strcmp(string[i], "[n]")) {
      key.mod |= DWB_NUMMOD_MASK;
    }
    else {
      g_string_append(buffer, string[i]);
    }
  }
  key.str = buffer->str;

  g_strfreev(string);
  g_string_free(buffer, false);

  return key;
}/*}}}*/

/* dwb_keymap_delete(GList *, KeyValue )     return: GList * {{{*/
static GList * 
dwb_keymap_delete(GList *gl, KeyValue key) {
  for (GList *l = gl; l; l=l->next) {
    KeyMap *km = l->data;
    if (!strcmp(km->map->n.first, key.id)) {
      gl = g_list_delete_link(gl, l);
      break;
    }
  }
  gl = g_list_sort(gl, (GCompareFunc)util_keymap_sort_second);
  return gl;
}/*}}}*/

/* dwb_keymap_add(GList *, KeyValue)     return: GList* {{{*/
GList *
dwb_keymap_add(GList *gl, KeyValue key) {
  gl = dwb_keymap_delete(gl, key);
  for (int i=0; i<LENGTH(FMAP); i++) {
    if (!strcmp(FMAP[i].n.first, key.id)) {
      KeyMap *keymap = dwb_malloc(sizeof(KeyMap));
      FunctionMap *fmap = &FMAP[i];
      keymap->key = key.key.str ? key.key.str : "";
      keymap->mod = key.key.mod;
      fmap->n.first = (char*)key.id;
      keymap->map = fmap;
      gl = g_list_prepend(gl, keymap);
      break;
    }
  }
  return gl;
}/*}}}*/
/*}}}*/

/* INIT {{{*/

/* dwb_init_key_map() {{{*/
static void 
dwb_init_key_map() {
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;
  dwb.keymap = NULL;

  g_key_file_load_from_file(keyfile, dwb.files.keys, G_KEY_FILE_KEEP_COMMENTS, &error);
  if (error) {
    fprintf(stderr, "No keyfile found: %s\nUsing default values.\n", error->message);
    g_clear_error(&error);
  }
  for (int i=0; i<LENGTH(KEYS); i++) {
    KeyValue kv;
    char *string = g_key_file_get_value(keyfile, dwb.misc.profile, KEYS[i].id, NULL);
    if (string) {
      kv.key = dwb_str_to_key(string);
      g_free(string);
    }
    else if (KEYS[i].key.str) {
      kv.key = KEYS[i].key;
    }
    else {
       kv.key.str = NULL;
       kv.key.mod = 0;
    }

    kv.id = KEYS[i].id;
    dwb.keymap = dwb_keymap_add(dwb.keymap, kv);
  }

  dwb.keymap = g_list_concat(dwb.keymap, dwb_get_scripts());
  dwb.keymap = g_list_sort(dwb.keymap, (GCompareFunc)util_keymap_sort_second);

  g_key_file_free(keyfile);
}/*}}}*/

/* dwb_read_settings() {{{*/
static gboolean
dwb_read_settings() {
  GError *error = NULL;
  gsize length, numkeys = 0;
  char  **keys = NULL;
  char  *content;
  GKeyFile  *keyfile = g_key_file_new();
  Arg *arg;
  setlocale(LC_NUMERIC, "C");

  g_file_get_contents(dwb.files.settings, &content, &length, &error);
  if (error) {
    fprintf(stderr, "No settingsfile found: %s\nUsing default values.\n", error->message);
    g_clear_error(&error);
  }
  else {
    g_key_file_load_from_data(keyfile, content, length, G_KEY_FILE_KEEP_COMMENTS, &error);
    if (error) {
      fprintf(stderr, "Couldn't read settings file: %s\nUsing default values.\n", error->message);
      g_clear_error(&error);
    }
    else {
      keys = g_key_file_get_keys(keyfile, dwb.misc.profile, &numkeys, &error); 
      if (error) {
        fprintf(stderr, "Couldn't read settings for profile %s: %s\nUsing default values.\n", dwb.misc.profile,  error->message);
        g_clear_error(&error);
      }
    }
  }
  FREE(content);
  for (int j=0; j<LENGTH(DWB_SETTINGS); j++) {
    gboolean set = false;
    char *key = g_strdup(DWB_SETTINGS[j].n.first);
    for (int i=0; i<numkeys; i++) {
      char *value = g_key_file_get_string(keyfile, dwb.misc.profile, keys[i], NULL);
      if (!strcmp(keys[i], DWB_SETTINGS[j].n.first)) {
        WebSettings *s = dwb_malloc(sizeof(WebSettings));
        *s = DWB_SETTINGS[j];
        if ( (arg = util_char_to_arg(value, s->type)) ) {
          s->arg = *arg;
        }
        g_hash_table_insert(dwb.settings, key, s);
        set = true;
      }
      FREE(value);
    }
    if (!set) {
      g_hash_table_insert(dwb.settings, key, &DWB_SETTINGS[j]);
    }
  }
  if (keys)
    g_strfreev(keys);
  return true;
}/*}}}*/

/* dwb_init_settings() {{{*/
static void
dwb_init_settings() {
  dwb.settings = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)dwb_free, NULL);
  dwb.state.web_settings = webkit_web_settings_new();
  dwb_read_settings();
  for (GList *l =  g_hash_table_get_values(dwb.settings); l; l = l->next) {
    WebSettings *s = l->data;
    if (s->apply & SETTING_BUILTIN || s->apply & SETTING_ONINIT) {
      s->func(NULL, s);
    }
  }
}/*}}}*/

/* dwb_init_scripts{{{*/
void 
dwb_init_scripts() {
  FREE(dwb.misc.scripts);
  GString *buffer = g_string_new(NULL);

  setlocale(LC_NUMERIC, "C");
  /* user scripts */
  util_get_directory_content(&buffer, dwb.files.scriptdir);

  /* systemscripts */
  char *dir = NULL;
  if ( (dir = util_get_data_dir("scripts")) ) {
    util_get_directory_content(&buffer, dir);
    g_free(dir);
  }
  g_string_append_printf(buffer, "DwbHintObj.init(\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%f\", %s);", 
      GET_CHAR("hint-letter-seq"),
      GET_CHAR("hint-font"),
      GET_CHAR("hint-style"), 
      GET_CHAR("hint-fg-color"), 
      GET_CHAR("hint-bg-color"), 
      GET_CHAR("hint-active-color"), 
      GET_CHAR("hint-normal-color"), 
      GET_CHAR("hint-border"), 
      GET_DOUBLE("hint-opacity"),
      GET_BOOL("hint-highlight-links") ? "true" : "false");
  dwb.misc.scripts = buffer->str;
  g_string_free(buffer, false);
}/*}}}*/

/* dwb_init_style() {{{*/
static void
dwb_init_style() {
  /* Colors  */
  /* Statusbar */
  DWB_COLOR_PARSE(&dwb.color.active_fg, GET_CHAR("active-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.active_bg, GET_CHAR("active-bg-color"));
  DWB_COLOR_PARSE(&dwb.color.normal_fg, GET_CHAR("normal-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.normal_bg, GET_CHAR("normal-bg-color"));

  /* Tabs */
  DWB_COLOR_PARSE(&dwb.color.tab_active_fg, GET_CHAR("tab-active-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.tab_active_bg, GET_CHAR("tab-active-bg-color"));
  DWB_COLOR_PARSE(&dwb.color.tab_normal_fg, GET_CHAR("tab-normal-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.tab_normal_bg, GET_CHAR("tab-normal-bg-color"));

  /* Downloads */
  DWB_COLOR_PARSE(&dwb.color.download_fg, "#ffffff");
  DWB_COLOR_PARSE(&dwb.color.download_bg, "#000000");

  /* SSL */
  DWB_COLOR_PARSE(&dwb.color.ssl_trusted, GET_CHAR("ssl-trusted-color"));
  DWB_COLOR_PARSE(&dwb.color.ssl_untrusted, GET_CHAR("ssl-untrusted-color"));

  DWB_COLOR_PARSE(&dwb.color.active_c_bg, GET_CHAR("active-completion-bg-color"));
  DWB_COLOR_PARSE(&dwb.color.active_c_fg, GET_CHAR("active-completion-fg-color"));
  DWB_COLOR_PARSE(&dwb.color.normal_c_bg, GET_CHAR("normal-completion-bg-color"));
  DWB_COLOR_PARSE(&dwb.color.normal_c_fg, GET_CHAR("normal-completion-fg-color"));

  DWB_COLOR_PARSE(&dwb.color.error, GET_CHAR("error-color"));

  dwb.color.tab_number_color = GET_CHAR("tab-number-color");
  dwb.color.allow_color = GET_CHAR("status-allowed-color");
  dwb.color.block_color = GET_CHAR("status-blocked-color");


  char *font = GET_CHAR("font");
  dwb.font.fd_active = pango_font_description_from_string(font);
  char *f;
#define SET_FONT(var, prop) f = GET_CHAR(prop); var = pango_font_description_from_string(f ? f : font)
  SET_FONT(dwb.font.fd_inactive, "font-inactive");
  SET_FONT(dwb.font.fd_entry, "font-entry");
  SET_FONT(dwb.font.fd_completion, "font-completion");
#undef SET_FONT

} /*}}}*/

/* dwb_init_gui() {{{*/
static void 
dwb_init_gui() {
  /* Window */
  dwb.gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_wmclass(GTK_WINDOW(dwb.gui.window), dwb.misc.name, dwb.misc.name);
  gtk_widget_set_name(dwb.gui.window, "dwb");
  /* Icon */
  GdkPixbuf *icon_pixbuf = gdk_pixbuf_new_from_xpm_data(icon);
  gtk_window_set_icon(GTK_WINDOW(dwb.gui.window), icon_pixbuf);

  gtk_window_set_default_size(GTK_WINDOW(dwb.gui.window), GET_INT("default-width"), GET_INT("default-height"));
  gtk_window_set_geometry_hints(GTK_WINDOW(dwb.gui.window), NULL, NULL, GDK_HINT_MIN_SIZE);
  g_signal_connect(dwb.gui.window, "delete-event", G_CALLBACK(dwb_end), NULL);
  g_signal_connect(dwb.gui.window, "key-press-event", G_CALLBACK(dwb_key_press_cb), NULL);
  g_signal_connect(dwb.gui.window, "key-release-event", G_CALLBACK(dwb_key_release_cb), NULL);
  DWB_WIDGET_OVERRIDE_BACKGROUND(dwb.gui.window, GTK_STATE_NORMAL, &dwb.color.active_bg);

  /* Main */
  dwb.gui.vbox = gtk_vbox_new(false, 1);
  dwb.gui.topbox = gtk_hbox_new(true, 1);
  dwb.gui.paned = gtk_hpaned_new();
  dwb.gui.left = gtk_vbox_new(true, 0);
  dwb.gui.right = gtk_vbox_new(true, 1);

  /* Paned */
  GtkWidget *paned_event = gtk_event_box_new(); 
  DWB_WIDGET_OVERRIDE_BACKGROUND(paned_event, GTK_STATE_NORMAL, &dwb.color.normal_bg);
  DWB_WIDGET_OVERRIDE_BACKGROUND(dwb.gui.paned, GTK_STATE_NORMAL, &dwb.color.normal_bg);
  DWB_WIDGET_OVERRIDE_BACKGROUND(dwb.gui.paned, GTK_STATE_PRELIGHT, &dwb.color.active_bg);
  gtk_container_add(GTK_CONTAINER(paned_event), dwb.gui.paned);
  /* Downloadbar */
  dwb.gui.downloadbar = gtk_hbox_new(false, 3);

  /* Pack */
  gtk_paned_pack1(GTK_PANED(dwb.gui.paned), dwb.gui.left, true, true);
  gtk_paned_pack2(GTK_PANED(dwb.gui.paned), dwb.gui.right, true, true);

  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.downloadbar, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), dwb.gui.topbox, false, false, 0);
  gtk_box_pack_start(GTK_BOX(dwb.gui.vbox), paned_event, true, true, 0);

  gtk_container_add(GTK_CONTAINER(dwb.gui.window), dwb.gui.vbox);

  gtk_widget_show(dwb.gui.left);
  gtk_widget_show(dwb.gui.paned);
  gtk_widget_show(paned_event);
  gtk_widget_show_all(dwb.gui.topbox);

  gtk_widget_show(dwb.gui.vbox);
  gtk_widget_show(dwb.gui.window);

} /*}}}*/

/* dwb_init_file_content {{{*/
GList *
dwb_init_file_content(GList *gl, const char *filename, Content_Func func) {
  char *content = util_get_file_content(filename);

  if (content) {
    char **token = g_strsplit(content, "\n", 0);
    int length = MAX(g_strv_length(token) - 1, 0);
    for (int i=0;  i < length; i++) {
      gl = g_list_append(gl, func(token[i]));
    }
    g_free(content);
    g_strfreev(token);
  }
  return gl;
}/*}}}*/

static Navigation * 
dwb_get_search_completion_from_navigation(Navigation *n) {
  char *uri = n->second;
  n->second = g_strdup(util_domain_from_uri(n->second));

  FREE(uri);
  return n;
}
static Navigation * 
dwb_get_search_completion(const char *text) {
  Navigation *n = dwb_navigation_new_from_line(text);
  return dwb_get_search_completion_from_navigation(n);
}

/* dwb_init_files() {{{*/
static void
dwb_init_files() {
  char *path           = util_build_path();
  char *profile_path   = g_build_filename(path, dwb.misc.profile, NULL);

  if (!g_file_test(profile_path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(profile_path, 0700);
  }
  char *cache_dir = g_build_filename(g_get_user_cache_dir(), dwb.misc.name, NULL);
  if (!g_file_test(cache_dir, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(cache_dir, 0700);
  }
  else {
    util_rmdir(cache_dir, true);
  }
  g_free(cache_dir);
  dwb.files.bookmarks     = g_build_filename(profile_path, "bookmarks",     NULL);
  dwb.files.history       = g_build_filename(profile_path, "history",       NULL);
  dwb.files.stylesheet    = g_build_filename(profile_path, "stylesheet",    NULL);
  dwb.files.quickmarks    = g_build_filename(profile_path, "quickmarks",    NULL);
  dwb.files.session       = g_build_filename(profile_path, "session",       NULL);
  dwb.files.searchengines = g_build_filename(path, "searchengines", NULL);
  dwb.files.keys          = g_build_filename(path, "keys",          NULL);
  dwb.files.scriptdir     = g_build_filename(path, "scripts",      NULL);
  dwb.files.userscripts   = g_build_filename(path, "userscripts",   NULL);
  dwb.files.settings      = g_build_filename(path, "settings",      NULL);
  dwb.files.mimetypes     = g_build_filename(path, "mimetypes",      NULL);
  dwb.files.cookies       = g_build_filename(profile_path, "cookies",       NULL);
  dwb.files.cookies_allow = g_build_filename(profile_path, "cookies.allow", NULL);
  dwb.files.adblock       = g_build_filename(path, "adblock",      NULL);
  dwb.files.scripts_allow  = g_build_filename(profile_path, "scripts.allow",      NULL);
  dwb.files.plugins_allow  = g_build_filename(profile_path, "plugins.allow",      NULL);

  if (!g_file_test(dwb.files.scriptdir, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(dwb.files.scriptdir, 0755);
  }

  if (!g_file_test(dwb.files.userscripts, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(dwb.files.userscripts, 0755);
  }

  dwb.fc.bookmarks = dwb_init_file_content(dwb.fc.bookmarks, dwb.files.bookmarks, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.history = dwb_init_file_content(dwb.fc.history, dwb.files.history, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.quickmarks = dwb_init_file_content(dwb.fc.quickmarks, dwb.files.quickmarks, (Content_Func)dwb_quickmark_new_from_line); 
  dwb.fc.searchengines = dwb_init_file_content(dwb.fc.searchengines, dwb.files.searchengines, (Content_Func)dwb_navigation_new_from_line); 
  dwb.fc.se_completion = dwb_init_file_content(dwb.fc.se_completion, dwb.files.searchengines, (Content_Func)dwb_get_search_completion);
  dwb.fc.mimetypes = dwb_init_file_content(dwb.fc.mimetypes, dwb.files.mimetypes, (Content_Func)dwb_navigation_new_from_line);
  dwb.fc.tmp_scripts = NULL;
  dwb.fc.tmp_plugins = NULL;

  if (g_list_last(dwb.fc.searchengines)) 
    dwb.misc.default_search = ((Navigation*)dwb.fc.searchengines->data)->second;
  else 
    dwb.misc.default_search = NULL;
  dwb.fc.cookies_allow = dwb_init_file_content(dwb.fc.cookies_allow, dwb.files.cookies_allow, (Content_Func)dwb_return);
  dwb.fc.adblock = dwb_init_file_content(dwb.fc.adblock, dwb.files.adblock, (Content_Func)dwb_return);

  FREE(path);
  FREE(profile_path);
}/*}}}*/

/* signals {{{*/
static void
dwb_handle_signal(int s) {
  if (s == SIGALRM || s == SIGFPE || s == SIGILL || s == SIGINT || s == SIGQUIT || s == SIGTERM) {
    dwb_end();
    exit(EXIT_SUCCESS);
  }
  else if (s == SIGSEGV) {
    fprintf(stderr, "Received SIGSEGV, trying to clean up.\n");
    session_save(NULL);
    exit(EXIT_FAILURE);
  }
}

static void 
dwb_init_signals() {
  for (int i=0; i<LENGTH(signals); i++) {
    struct sigaction act, oact;
    act.sa_handler = dwb_handle_signal;

    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(signals[i], &act, &oact);
  }
}/*}}}*/

/* dwb_init_vars{{{*/
static void 
dwb_init_vars() {
  dwb.misc.message_delay = GET_INT("message-delay");
  dwb.misc.history_length = GET_INT("history-length");
  dwb.misc.factor = GET_DOUBLE("factor");
  dwb.misc.startpage = GET_CHAR("startpage");
  dwb.misc.tabbed_browsing = GET_BOOL("tabbed-browsing");
  dwb.misc.private_browsing = GET_BOOL("enable-private-browsing");
  dwb.misc.scroll_step = GET_DOUBLE("scroll-step");
  dwb.misc.top_statusbar = GET_BOOL("top-statusbar");
  dwb.state.tabbar_visible = dwb_eval_tabbar_visible(GET_CHAR("hide-tabbar"));
  dwb.state.cookies_allowed = GET_BOOL("cookies");

  dwb.state.complete_history = GET_BOOL("complete-history");
  dwb.state.complete_bookmarks = GET_BOOL("complete-bookmarks");
  dwb.state.complete_searchengines = GET_BOOL("complete-searchengines");
  dwb.state.complete_commands = GET_BOOL("complete-commands");
  dwb.state.complete_userscripts = GET_BOOL("complete-userscripts");
  dwb.state.background_tabs = GET_BOOL("background-tabs");

  dwb.state.size = GET_INT("size");
  dwb.state.layout = dwb_layout_from_char(GET_CHAR("layout"));
  dwb.comps.autocompletion = GET_BOOL("auto-completion");
}/*}}}*/

char *
dwb_get_stock_item_base64_encoded(const char *name) {
  GdkPixbuf *pb;
  char *ret = NULL;
#if _HAS_GTK3
  pb = gtk_widget_render_icon_pixbuf(dwb.gui.window, name, -1);
#else 
  pb = gtk_widget_render_icon(dwb.gui.window, name, -1, NULL);
#endif
  if (pb) {
    char *buffer;
    size_t buffer_size;
    gboolean success = gdk_pixbuf_save_to_buffer(pb, &buffer, &buffer_size, "png", NULL, NULL);
    if (success) {
      char *encoded = g_base64_encode((unsigned char*)buffer, buffer_size);
      ret = g_strdup_printf("data:image/png;base64,%s", encoded);
      g_free(encoded);
      g_free(buffer);
    }
    g_object_unref(pb);
  }
  return ret;
}

static void 
dwb_tab_size_cb(GtkWidget *w, GtkAllocation *a, GList *gl) {
  dwb.misc.tab_height = a->height;
  g_signal_handlers_disconnect_by_func(w, dwb_tab_size_cb, NULL);
}

/* dwb_init() {{{*/
static void 
dwb_init() {
  dwb_clean_vars();
  dwb.state.views = NULL;
  dwb.state.fview = NULL;
  dwb.state.last_cookie = NULL;
  dwb.state.last_cookies = NULL;
  dwb.state.fullscreen = false;

  dwb.comps.completions = NULL; 
  dwb.comps.active_comp = NULL;

  dwb.misc.max_c_items = MAX_COMPLETIONS;
  dwb.misc.userscripts = NULL;
  dwb.misc.proxyuri = NULL;
  dwb.misc.scripts = NULL;
  dwb.misc.synctimer = 0;

  char *path = util_get_data_file(PLUGIN_FILE);
  dwb.misc.pbbackground = util_get_file_content(path);


  dwb_init_key_map();
  dwb_init_style();
  dwb_init_gui();
  dwb_init_scripts();

  dwb_soup_init();
  dwb_init_vars();

  if (dwb.state.layout & BOTTOM_STACK) {
    Arg a = { .n = dwb.state.layout };
    commands_set_orientation(NULL, &a);
  }
  if (restore && session_restore(restore));
  else if (dwb.misc.argc > 0) {
    for (int i=0; i<dwb.misc.argc; i++) {
      Arg a = { .p = dwb.misc.argv[i] };
      view_add(&a, false);
    }
  }
  else {
    view_add(NULL, false);
  }
  g_signal_connect(VIEW(dwb.state.fview)->tablabel, "size-allocate", G_CALLBACK(dwb_tab_size_cb), NULL);
  dwb_toggle_tabbar();
} /*}}}*/ /*}}}*/

/* FIFO {{{*/
/* dwb_parse_command_line(const char *line) {{{*/
void 
dwb_parse_command_line(const char *line) {
  char **token = g_strsplit(line, " ", 2);
  KeyMap *m;

  if (!token[0]) 
    return;

  for (GList *l = dwb.keymap; l; l=l->next) {
    m = l->data;
    if (!strcmp(m->map->n.first, token[0])) {
      if (m->map->prop & CP_HAS_MODE) 
        dwb_change_mode(NORMAL_MODE, true);
      Arg a = m->map->arg;
      if (token[1]) {
        g_strstrip(token[1]);
        m->map->arg.p = token[1];
      }
      if (gtk_widget_has_focus(dwb.gui.entry) && m->map->entry) {
        m->map->func(&m, &m->map->arg);
      }
      else {
        commands_simple_command(m);
      }
      m->map->arg = a;
      break;
    }
  }
  g_strfreev(token);
  if (!(m->map->prop & CP_HAS_MODE)) {
    dwb_change_mode(NORMAL_MODE, true);
  }
}/*}}}*/

/* dwb_handle_channel {{{*/
static gboolean
dwb_handle_channel(GIOChannel *c, GIOCondition condition, void *data) {
  char *line = NULL;

  g_io_channel_read_line(c, &line, NULL, NULL, NULL);
  if (line) {
    g_strstrip(line);
    dwb_parse_command_line(line);
    g_io_channel_flush(c, NULL);
    g_free(line);
  }
  return true;
}/*}}}*/

/* dwb_init_fifo{{{*/
static void 
dwb_init_fifo(int single) {
  FILE *ff;

  /* Files */
  char *path = util_build_path();
  dwb.files.unifile = g_build_filename(path, "dwb-uni.fifo", NULL);


  dwb.misc.si_channel = NULL;
  if (single == NEW_INSTANCE) {
    return;
  }

  if (GET_BOOL("single-instance")) {
    if (!g_file_test(dwb.files.unifile, G_FILE_TEST_EXISTS)) {
      mkfifo(dwb.files.unifile, 0666);
    }
    int fd = open(dwb.files.unifile, O_WRONLY | O_NONBLOCK);
    if ( (ff = fdopen(fd, "w")) ) {
      if (dwb.misc.argc) {
        for (int i=0; i<dwb.misc.argc; i++) {
          if (g_file_test(dwb.misc.argv[i], G_FILE_TEST_EXISTS) && !g_path_is_absolute(dwb.misc.argv[i])) {
            char *curr_dir = g_get_current_dir();
            path = g_build_filename(curr_dir, dwb.misc.argv[i], NULL);

            fprintf(ff, "add_view %s\n", path);

            FREE(curr_dir);
            FREE(path);
          }
          else {
            fprintf(ff, "add_view %s\n", dwb.misc.argv[i]);
          }
        }
      }
      else {
        fprintf(ff, "add_view\n");
      }
      fclose(ff);
      exit(EXIT_SUCCESS);
    }
    close(fd);
    dwb_open_si_channel();
  }

  /* fifo */
  if (GET_BOOL("use-fifo")) {
    char *filename = g_strdup_printf("%s-%d.fifo", dwb.misc.name, getpid());
    dwb.files.fifo = g_build_filename(path, filename, NULL);
    FREE(filename);

    if (!g_file_test(dwb.files.fifo, G_FILE_TEST_EXISTS)) {
      mkfifo(dwb.files.fifo, 0600);
    }
    dwb_open_channel(dwb.files.fifo);
  }

  FREE(path);
}/*}}}*/
/*}}}*/

int 
main(int argc, char *argv[]) {
  dwb.misc.name = REAL_NAME;
  dwb.misc.profile = "default";
  dwb.misc.argc = 0;
  dwb.misc.prog_path = argv[0];
  int last = 0;
  int single = 0;
  int argr = argc;
 

  gtk_init(&argc, &argv);

  if (argc > 1) {
    for (int i=1; i<argc; i++) {
      if (argv[i][0] == '-') {
        if (argv[i][1] == 'l') {
          session_list();
          argr--;
        }
        else if (argv[i][1] == 'p' && argv[i++]) {
          dwb.misc.profile = argv[i];
          argr -= 2;
        }
        else if (argv[i][1] == 'n') {
          single = NEW_INSTANCE;
          argr -=1;
        }
        else if (argv[i][1] == 'r' ) {
          if (!argv[i+1] || argv[i+1][0] == '-') {
            restore = "default";
            argr--;
          }
          else {
            restore = argv[++i];
            argr -=2;
          }
        }
        else if (argv[i][1] == 'v') {
          printf("%s %s, %s\n", NAME, VERSION, COPYRIGHT);
          return 0;
        }
      }
      else {
        last = i;
        break;
      }
    }
  }
  dwb_init_files();
  dwb_init_settings();
  if (GET_BOOL("save-session") && argr == 1 && !restore && single != NEW_INSTANCE) {
    restore = "default";
  }

  if (last) {
    dwb.misc.argv = &argv[last];
    dwb.misc.argc = g_strv_length(dwb.misc.argv);
  }
  dwb_init_fifo(single);
  dwb_init_signals();
  dwb_init();
  gtk_main();
  return EXIT_SUCCESS;
}
