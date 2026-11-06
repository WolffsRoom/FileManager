import sys

with open('c:/Users/wolff/Documents/SDKVita/VitaShell-master/browser.c', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Double tap threshold
content = content.replace('if (last_tap_index == clicked_index && (current_time - last_tap_time) < 500000) {',
                          'if (last_tap_index == clicked_index && (current_time - last_tap_time) < 800000) {')

# 2. Global Clipping Fix
clip_fix_old = "          vita2d_set_clip_rectangle(x + 1.0f, y, x + 1.0f + max_w, y + FONT_Y_SPACE);"
clip_fix_new = """          float clip_y = (y < START_Y) ? START_Y : y;
          float clip_h = (y + FONT_Y_SPACE > SCREEN_HEIGHT - 60) ? (SCREEN_HEIGHT - 60) : (y + FONT_Y_SPACE);
          vita2d_set_clip_rectangle(x + 1.0f, clip_y, x + 1.0f + max_w, clip_h);"""
content = content.replace(clip_fix_old, clip_fix_new)

# 3. Finder mode parent list drawing fix
gp_old = """        FileListEntry *gp_entry = grandparent_list.head;
        int p_i = 0; float p_y = START_Y;
        while (gp_entry && p_i < 13) {"""
        
gp_new = """        FileListEntry *gp_entry = grandparent_list.head;
        int p_i = 0; float p_y = START_Y;
        
        char curr_parent[256];
        snprintf(curr_parent, 256, "%s", file_list.path);
        int l = strlen(curr_parent);
        if (l > 0 && curr_parent[l-1] == '/') curr_parent[l-1] = '\\0';
        char *s = strrchr(curr_parent, '/');
        if (s) *s = '\\0';
        s = strrchr(curr_parent, '/');
        if (!s) s = strchr(curr_parent, ':');
        if (s) snprintf(curr_parent, 256, "%s", s + 1);

        int gp_target = 0;
        FileListEntry *temp = gp_entry;
        while (temp) {
            if (strcmp(temp->name, curr_parent) == 0) break;
            gp_target++;
            temp = temp->next;
        }
        int gp_start = (gp_target > 5) ? gp_target - 5 : 0;
        for (int skip = 0; skip < gp_start && gp_entry; skip++) gp_entry = gp_entry->next;
        
        while (gp_entry && p_i < 13) {"""
content = content.replace(gp_old, gp_new)

p_old = """        FileListEntry *p_entry = parent_list.head;
        int p_i = 0; float p_y = START_Y;
        while (p_entry && p_i < 13) {"""
        
p_new = """        FileListEntry *p_entry = parent_list.head;
        int p_i = 0; float p_y = START_Y;
        
        char curr_folder[256];
        snprintf(curr_folder, 256, "%s", file_list.path);
        int len = strlen(curr_folder);
        if (len > 0 && curr_folder[len-1] == '/') curr_folder[len-1] = '\\0';
        char *slash = strrchr(curr_folder, '/');
        if (!slash) slash = strchr(curr_folder, ':');
        if (slash) snprintf(curr_folder, 256, "%s", slash + 1);
        
        int p_target = 0;
        FileListEntry *temp2 = p_entry;
        while (temp2) {
            if (strcmp(temp2->name, curr_folder) == 0) break;
            p_target++;
            temp2 = temp2->next;
        }
        int p_start = (p_target > 5) ? p_target - 5 : 0;
        for (int skip = 0; skip < p_start && p_entry; skip++) p_entry = p_entry->next;
        
        while (p_entry && p_i < 13) {"""
content = content.replace(p_old, p_new)

# 4. Clipping for parent and grandparent inner loops
# Inner loop clipping replace:
p_clip_old = """            vita2d_set_clip_rectangle(ox + 28.0f, p_y, ox + 280.0f, p_y + FONT_Y_SPACE);"""
p_clip_new = """            float clip_p_h = (p_y + FONT_Y_SPACE > SCREEN_HEIGHT - 60) ? (SCREEN_HEIGHT - 60) : (p_y + FONT_Y_SPACE);
            vita2d_set_clip_rectangle(ox + 28.0f, p_y, ox + 280.0f, clip_p_h);"""
content = content.replace(p_clip_old, p_clip_new)

gp_clip_old = """            vita2d_set_clip_rectangle(SHELL_MARGIN_X + 28.0f, p_y, SHELL_MARGIN_X + 280.0f, p_y + FONT_Y_SPACE);"""
gp_clip_new = """            float clip_gp_h = (p_y + FONT_Y_SPACE > SCREEN_HEIGHT - 60) ? (SCREEN_HEIGHT - 60) : (p_y + FONT_Y_SPACE);
            vita2d_set_clip_rectangle(SHELL_MARGIN_X + 28.0f, p_y, SHELL_MARGIN_X + 280.0f, clip_gp_h);"""
content = content.replace(gp_clip_old, gp_clip_new)

with open('c:/Users/wolff/Documents/SDKVita/VitaShell-master/browser.c', 'w', encoding='utf-8') as f:
    f.write(content)
print('Patch applied successfully!')
