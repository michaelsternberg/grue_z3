BIN = ../../bin
LIBS = Grammar.h linklpa.h linklv.h Parser.h parserm.h VerbLib.h verblibm.h

grue.z3: grue.inf $(LIBS) ../../bin/inform
	$(BIN)/inform -v3 grue.inf
	mv grue.z3 $(BIN)

$(BIN)/inform: force_look
	cd ../inform615; $(MAKE)

force_look:
	true
