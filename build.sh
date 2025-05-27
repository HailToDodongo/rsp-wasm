set -e

clang++ -nostdlib --target=wasm32 -std=c++20 \
  -O3 \
  -msign-ext \
  -mmultivalue \
  -mbulk-memory \
  -msimd128 \
  -mnontrapping-fptoint \
  -Wall \
  -flto -pedantic-errors -fno-exceptions -fno-rtti \
  -Wno-logical-op-parentheses \
  -Wno-shift-op-parentheses \
  -Wno-bitwise-op-parentheses \
  -Wno-gnu-anonymous-struct \
  \
  -Wl,--no-entry \
  -Wl,--export-dynamic \
  -Wl,--fatal-warnings \
  -Wl,--allow-undefined \
  -Wl,--lto-O3 \
  \
  -o rsp.wasm \
  src/main.cpp

# mv code.wasm ../public
