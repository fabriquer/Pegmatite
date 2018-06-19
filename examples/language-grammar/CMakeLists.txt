# Ignore pedantic warnings in this simple example
add_definitions("-Wno-non-virtual-dtor")

add_executable(repl language-grammar.cc)
target_link_libraries(repl pegmatite-static)
