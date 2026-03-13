import sys
import re

def replace_in_file(filename):
    with open(filename, 'r') as f:
        content = f.read()

    # We want to match `*reinterpret_cast<TYPE*>(&VAR)`
    # The `TYPE` can be `T`, `double`, `const double`, `UInt`, `float`, etc.
    # The result should be `([&](){ TYPE res; std::memcpy(&res, &VAR, sizeof(TYPE)); return res; })()`

    def repl(m):
        type_str = m.group(1).strip()
        var_str = m.group(2).strip()
        
        # Remove 'const' if it's there
        if type_str.startswith('const '):
            type_str = type_str[6:]
            
        return f'([&](){{ {type_str} res; std::memcpy(&res, &{var_str}, sizeof({type_str})); return res; }})()'

    # pattern for single line:
    content = re.sub(r'\*reinterpret_cast<([^>]+)\*>\(\s*&([a-zA-Z0-9_:<>]+)\s*\)', repl, content)

    # pattern for multiline (specifically the one in Sqrt):
    # `*reinterpret_cast<const double*>(\n          &FPTypeInfo<double>::kCanonicalNaN)`
    content = re.sub(r'\*reinterpret_cast<([^>]+)\*>\(\s*\n\s*&([a-zA-Z0-9_:<>]+)\s*\)', repl, content)

    with open(filename, 'w') as f:
        f.write(content)

replace_in_file(sys.argv[1])
