#!/usr/bin/env sh
set -evx

clang-format --version

git ls-files -- '*Trigger*.*pp' '*Test*.*pp' '*_guarded*.*pp' '*Benchmarks*.cpp'| xargs clang-format -sort-includes -i -style=file

git diff --exit-code --color

set +evx
