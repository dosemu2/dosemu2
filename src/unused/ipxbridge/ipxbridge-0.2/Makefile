
# Define ETH0_ETH1 if the two interfaces are eth0 and eth1. 
# (Assumption is that they have 0 or 1 in  4th char position. 'eth' itself
# is irrelevant.  In general, any two interfaces which differ in single
# position can be used. Use CHANGED_POS to define that position.
# Otherwise, define the two interfaces DEVICE0 and DEVICE1.

ETHOPT=-DETH0_ETH1 -DCHANGED_POS=4
#DEBUGOPT=-DDEBUG -DLDEBUG

#DEVICE0=-DDEVICE0=\"eth0\"
#DEVICE1=-DDEVICE1=\"eth1\"

ipxbridge : ipxbridge.c Makefile
	gcc -Wall -O2 ipxbridge.c -o ipxbridge $(ETHOPT) $(DEBUGOPT) $(DEVICE0) $(DEVICE1) 

