a = [1, 2]

try:
    c = a[5]
except IndexError:
    c = 666

print(c)