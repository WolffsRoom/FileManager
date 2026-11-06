def trace_braces():
    with open('browser.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # Find the real browserMain definition:
    # "int browserMain(" at the start of a line
    start_anchor = "\nint browserMain("
    start_pos = content.find(start_anchor)
    if start_pos == -1:
        start_anchor = "int browserMain("
        start_pos = content.find(start_anchor)

    # Let's count newlines before start_pos to find the start line number
    start_line = content[:start_pos].count('\n') + 1
    print(f"Analyzing from start line {start_line} (pos {start_pos})...")

    # Stack of (line, col)
    stack = []
    
    line = 1
    col = 1
    
    in_string = False
    in_char = False
    in_line_comment = False
    in_block_comment = False
    
    i = start_pos
    total_len = len(content)
    
    while i < total_len:
        c = content[i]
        
        # Track line/col
        next_line = start_line + content[start_pos:i].count('\n')
        next_col = col + 1 # Simple approximation
        if c == '\n':
            next_col = 1
            
        if in_line_comment:
            if c == '\n':
                in_line_comment = False
        elif in_block_comment:
            if c == '/' and i > 0 and content[i-1] == '*':
                in_block_comment = False
        elif in_string:
            if c == '"' and i > 0 and content[i-1] != '\\':
                in_string = False
        elif in_char:
            if c == "'" and i > 0 and content[i-1] != '\\':
                in_char = False
        else:
            if c == '/' and i + 1 < total_len and content[i+1] == '/':
                in_line_comment = True
                i += 1
            elif c == '/' and i + 1 < total_len and content[i+1] == '*':
                in_block_comment = True
                i += 1
            elif c == '"':
                in_string = True
            elif c == "'":
                in_char = True
            elif c == '{':
                stack.append((next_line, col))
            elif c == '}':
                if len(stack) == 0:
                    print(f"ERROR: Extra closing brace '}}' at line {next_line}!")
                else:
                    popped = stack.pop()
                    if next_line >= 1430 and next_line <= 1520:
                        print(f"Close '}}' at line {next_line} pairs with Open '{{' at line {popped[0]}")
                    
        col = next_col
        i += 1

if __name__ == '__main__':
    trace_braces()
