# this is example-file: src/glint2/Makefile.am

include ../commonmk.am

#install-data-local :
#	$(INSTALL_DATA) glint2_modele.mod $(DESTDIR)$(includedir)

###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################

# the library names to build (note we are building static libs only)
lib_LTLIBRARIES = libglint2.la

# the sources to add to the library and to add to the source distribution
libglint2_la_SOURCES = \
	$(FORTRAN_SOURCES) \
	giss/geodesy.cpp \
	giss/IndexTranslator.cpp \
	giss/IndexTranslator2.cpp \
	giss/ncutil.cpp \
	giss/Proj2.cpp \
	giss/SparseMatrix.cpp \
	giss/sparsemult.cpp \
	glint2/clippers.cpp \
	glint2/ExchangeGrid.cpp \
	glint2/GCMCoupler.cpp \
	glint2/Grid.cpp \
	glint2/Grid_LonLat.cpp \
	glint2/Grid_XY.cpp \
	glint2/GridDomain.cpp \
	glint2/GridDomain_Listed.cpp \
	glint2/gridutil.cpp \
	glint2/HCIndex.cpp \
	glint2/IceModel.cpp \
	glint2/IceModel_Decode.cpp \
	glint2/IceModel_DISMAL.cpp \
	glint2/IceSheet.cpp \
	glint2/IceSheet_L0.cpp \
	glint2/matrix_ops.cpp \
	glint2/MatrixMaker.cpp \
	glint2/modele/grids_ll.cpp \
	glint2/modele/glint2_modele.cpp \
	glint2/read_grid.cpp \
	glint2/read_icemodel.cpp \
	glint2/read_matrixmaker.cpp \
	glint2/pism/PSConstantGLINT2.cpp \
	glint2/pism/IceModel_PISM.cpp \
	glint2/pism/PISMIceModel.cpp

# Deprecated, at least temporarily
#	glint2/IceModel_TConv.cpp


FORTRAN_SOURCES = \
	giss/f90blitz_f.f90 \
	glint2/modele/glint2_modele_f.f90

if USE_GALAHAD
FORTRAN_SOURCES += \
	galahad/eqp_f.f90 \
	galahad/ncutil_f.f90 \
	galahad/qpt_f.f90 \
	galahad/x_loc_f.f90 \
	galahad/zd11_f.f90

libglint2_la_SOURCES += \
	galahad/zd11_c.cpp \
	galahad/qpt_c.cpp

endif

# # ------------------------------------------------------
giss/f90blitz_f.f90 giss/f90blitz_f.h : giss/f90blitz_f.py
	python giss/f90blitz_f.py giss/f90blitz_f.f90 giss/f90blitz_f.h

-include f90_deps.mk
FORTRAN_COMMAND_LINE = $(FCCOMPILE) -c -o

deps : f90_deps.mk
.PHONY : all deps

f90_deps.mk : ../f90_mod_deps.py $(FORTRAN_SOURCES)
	python ../f90_mod_deps.py $(FORTRAN_SOURCES) >f90_deps.mk

#FORTRAN_SOURCES = giss/f90blitz_f.f90
#libglint2_la_SOURCES += $(FORTRAN_SOURCES)

# Fortran headers (.mod files) are installed from this directory.
# For C/C++ header files, see ../include

#libglint2_la_HEADERS = $(filter-out eqp_f.mod,$(FORTRAN_SOURCES:.f90=.mod))
# We should really use a Python script to figure out which module files are created.
# See ../f90_mod_deps.py
#libglint2_la_HEADERS = f90blitz.mod glint2_modele.mod $(shell find -E . -regex '.*\.(hpp|h)' )

nobase_include_HEADERS = \
	$(shell find . -name '*.hpp') \
	$(shell find . -name '*.h') \
	ncutil_mod.mod qpt_x.mod c_loc_x.mod zd11_x.mod \
	f90blitz.mod glint2_modele.mod

#	$(shell find . -name '*.mod')


libglint2_ladir = $(includedir)
