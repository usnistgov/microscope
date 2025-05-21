#############################################################################
# Makefile for cleaning your microscope directory
#############################################################################

DEL_FILE      = rm -f
TARGET        = microscope

####### Build rules


clean: 
	-$(DEL_FILE) *.o moc_*.cpp *.h
	-$(DEL_FILE) *~ core *.core
	-$(DEL_FILE) $(TARGET) 
	-$(DEL_FILE) .qmake.stash

