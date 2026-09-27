value = 2

if (value == 2) {
    echo control-ok
}

build(name) {
    echo name
    return 0
}

build("function-ok")
alias hi = echo
hi "alias-ok"
echo `printf "substitution-ok"`
printf "one\\ntwo\\n" > "/tmp/cshell-integration"
cat "/tmp/cshell-integration" | wc -l
sleep 1 &
echo integration-ok
rm "/tmp/cshell-integration"
