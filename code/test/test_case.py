import sys
import json


def readfile(path):
    return open(path)


def main():
    if len(sys.argv) <= 1:
        print("no test case file")
        return 0
    print(sys.argv[1])
    with open(sys.argv[1], "r") as json_file:
        data = json.load(json_file)
        print(data)


if __name__ == "__main__":
    main()
