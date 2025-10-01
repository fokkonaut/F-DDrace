from random import choice, randint
from os import popen2

masterservers = ["localhost 8300"]

maps = [
	["dm1", "dm2", "dm6"],
	["dm1", "dm2", "dm6"],
	["ctf1", "ctf2", "ctf3"],
]

servernames = [
	"%s playhouse",
	"%s own server",
]

nicks = []
for nick in open("scripts/nicks.txt"):
	nicks += nick.replace(":port80c.se.quakenet.org 353 matricks_ = #pcw :", "").strip().split()
inick = 0

def get_nick():
	global inick, nicks
	inick = (inick+1)%len(nicks)
	return nicks[inick].replace("`", "\`")
	
for s in range(0, 350):
	cmd = "./fake_server_d_d "
	cmd += '-n "%s" ' % (choice(servernames) % get_nick())
	for m in masterservers:
		cmd += '-m %s '%m
	
	max = randint(2, 16)
	cmd += "-x %d " % max
	
	t = randint(0, 2)

	cmd += f'-a "{choice(maps[t])}" '
	cmd += f'-g {randint(0, 100)} '
	cmd += f'-t {t} ' # dm, tdm, ctf
	cmd += f'-f {randint(0, 1)} ' # password protected

	for p in range(0, randint(0, max)):
		cmd += f'-p "{get_nick()}" {randint(0, 20)} '

	print(cmd)
	popen2(cmd)

