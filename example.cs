#!/bin/cs

name = "world"
counter = 0

echo "Hello" name

for (counter = 0; counter < 5; ++counter) {
	echo counter
}

if (counter == 5) {
	echo "finished"
} else {
	echo "failed"
}

switch (counter) {
	case 5: {
		echo "five"
	}
	default: {
		echo "other"
	}
}

say(text) {
	echo "message:" text
		return 0
}

say("done")
