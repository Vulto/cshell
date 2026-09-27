x = 0
for (x = 0; x < 3; ++x) {
    echo x
}
if (x == 3) {
    echo "done"
} else {
    echo "bad"
}
switch (x) {
case 2: {
    echo "two"
}
case 3: {
    echo "three"
}
default: {
    echo "other"
}
}

hello(name) {
    echo "Hello" name
    return 7
}
hello("world")
