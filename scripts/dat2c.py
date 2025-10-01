from sys import argv

data = open(argv[1], "rb").read() # In python 3 no more file(), use open()

i = 0
print ("unsigned char", argv[2], "[] = {")
print (str(ord(data[0])),)
for d in data[1:]:
	s = ","+str(ord(d))
	print (s)
	i += len(s)+1

	if i >= 70:
		print ("")
		i = 0
print ("")
print ("};")
