files = [
    "main.c",
    "util.c",
    "test.c"
]
echo files[0]
files[1] = "other.c"
echo files[1]
echo files.length
for (i = 0; i < files.length; ++i) {
    echo files[i]
}
