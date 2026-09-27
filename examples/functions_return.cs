check(value) {
    if (value == 1) {
        echo "returned"
        return 42
    }
    echo "not-returned"
}
check(1)
