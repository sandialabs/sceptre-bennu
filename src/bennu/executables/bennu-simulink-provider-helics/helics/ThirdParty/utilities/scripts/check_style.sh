#!/usr/bin/env sh
set -evx

clang-format --version

git ls-files -- '*Lock*.*pp' '*Test*.*pp' '*Buffer*.*pp' '*Vector*.hpp' '*Deque*.hpp' '*Queue*.*pp' '*Benchmarks*.cpp'| xargs clang-format -sort-includes -i -style=file

git diff --exit-code --color

set +evx
