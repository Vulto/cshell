i = 0
while (i < 5) {
    ++i
    if (i == 2) {
        continue
    }
    if (i == 4) {
        break
    }
    echo i
}
