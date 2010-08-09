static KeyValue KEYS[] = {
  { "add_view",                 {   "ga",         0,                  },  },  
  { "allow_cookie",             {   "CC",         0,                   },  },  
  { "bookmark",                 {   "M",         0,                   },  },  
  { "command_mode",             {   ":",         0,                   },  },  
  { "find_forward",             {   "/",         0,                   },  }, 
  { "find_next",                {   "n",         0,                   },  },
  { "find_previous",            {   "N",         0,                   },  },
  { "find_backward",            {   "?",         0,                   },  },  
  { "focus_next",               {   "J",         0,                   },  },  
  { "focus_prev",               {   "K",         0,                   },  },  
  { "hint_mode",                {   "f",         0,                   },  },  
  { "hint_mode_nv",             {   "F",         0,                   },  },  
  { "hint_mode_nv",             {   "wf",         0,                   },  },  
  { "history_back",             {   "H",         0,                   },  },  
  { "history_forward",          {   "L",         0,                   },  },  
  { "insert_mode",              {   "i",         0,                   },  },  
  { "increase_master",          {   "gl",         0,                  },  },  
  { "decrease_master",          {   "gh",         0,                  },  },  
  { "open",                     {   "o",         0,                   },  },  
  { "open_nv",                  {   "O",         0,                   },  },  
  { "open_nw",                  {   "wo",         0,                   },  },  
  { "open_quickmark",           {   "b",         0,                   },  },  
  { "open_quickmark_nv",        {   "B",         0,                   },  },  
  { "open_quickmark_nw",        {   "wb",        0,                   },  },  
  { "push_master",              {   "gp",        0,    },  },  
  { "reload",                   {   "r",         0,                   },  },  
  { "remove_view",              {   "d",         0,                   },  },  
  { "save_quickmark",           {   "m",         0,                   },  },  
  { "scroll_bottom",            {   "G",         0,                   },  },  
  { "scroll_down",              {   "j",         0,                   },  },  
  { "scroll_left",              {   "h",         0,                   },  },  
  { "scroll_page_down",         {   "f",         GDK_CONTROL_MASK,    },  },  
  { "scroll_page_up",           {   "b",         GDK_CONTROL_MASK,    },  },  
  { "scroll_right",             {   "l",         0,                   },  },  
  { "scroll_top",               {   "gg",         0,                  },  },  
  { "scroll_up",                {   "k",         0,                   },  },  
  { "show_keys",                {   "Sk",         0,                  },  },  
  { "show_settings",            {   "Ss",         0,                  },  },  
  { "show_global_settings",     {   "Sgs",         0,                  },  },  
  { "toggle_bottomstack",       {   "go",         0,                  },  },  
  { "toggle_maximized",         {   "gm",         0,                  },  },  
  { "view_source",              {   "gf",         0,                  },  },  
  { "zoom_in",                  {   "zi",         0,                  },  },  
  { "zoom_normal",              {   "z=",         0,                  },  },  
  { "zoom_out",                 {   "zo",         0,                  },  },  
  { "save_search_field",        {   "gs",         0,                  },  },  
  // settings
  { "autoload_images",          {   NULL,           0,                  },  },
  { "autoresize_window",        {   NULL,           0,                  },  },
  { "autoshrink_images",        {   NULL,           0,                  },  },
  { "caret_browsing",           {   NULL,           0,                  },  },
  { "java_applets",             {   NULL,           0,                  },  },
  { "plugins",                  {   NULL,           0,                  },  },
  { "private_browsing",         {   NULL,           0,                  },  },
  { "scripts",                  {   NULL,           0,                  },  },
  { "spell_checking",           {   NULL,           0,                  },  },
  { "proxy",                    {   "p" ,           GDK_CONTROL_MASK,  },  },
  { "toggle_encoding",          {   "te" ,           0,  },  },
  { "focus_input",              {   "gi",           0, }, }, 
  { "set_setting",              {   "ss",           0, }, }, 
  { "set_global_setting",       {   "sgs",           0, }, }, 
  { "set_key",                  {   "sk",           0, }, }, 
  { "yank",                     {   "yy",           0, }, }, 
  { "yank_primary",             {   "yY",           0, }, }, 
  { "paste",                    {   "pp",           0, }, }, 
  { "paste_primary",            {   "pP",           0, }, }, 
  { "paste_nv",                 {   "Pp",           0, }, }, 
  { "paste_primary_nv",         {   "PP",           0, }, }, 
  { "paste_nv",                 {   "wp",           0, }, }, 
  { "paste_primary_nv",         {   "wP",           0, }, }, 
  { "entry_delete_word",        {   "w",            GDK_CONTROL_MASK, }, }, 
  { "entry_delete_letter",      {   "h",            GDK_CONTROL_MASK, }, }, 
  { "entry_delete_line",        {   "u",            GDK_CONTROL_MASK, }, }, 
  { "entry_word_forward",       {   "f",            GDK_CONTROL_MASK, }, }, 
  { "entry_word_back",          {   "b",            GDK_CONTROL_MASK, }, }, 
  { "entry_history_forward",    {   "j",            GDK_CONTROL_MASK, }, }, 
  { "entry_history_back",       {   "k",            GDK_CONTROL_MASK, }, }, 
  { "download_set_execute",     {   "x",            GDK_CONTROL_MASK, }, }, 
  { "download_hint",            {   "gd",           0 }, },
  { "save_session",             {   "ZZ",           0 }, }, 
  { "save_named_session",       {   "gZZ",           0 }, }, 
};
