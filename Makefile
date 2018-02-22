BIN	 	= ./bin
ZIP		= $(BIN)/info3m.bin

DATE    = 171019
REL		= 1
DSK		= grue_$(DATE)_r$(REL)_z3m_apple2.dsk

all:	$(DSK)

$(DSK):	$(BIN)/grue.z3 $(BIN)/interlz3 $(BIN)/inform
	$(BIN)/interlz3 $(ZIP) $(BIN)/grue.z3 $(DSK)

$(BIN)/interlz3: force_look
	cd src/interlz3; $(MAKE)

$(BIN)/grue.z3: force_look
	cd src/story; $(MAKE)

force_look:
	true
