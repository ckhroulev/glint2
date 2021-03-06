# To turn of GCC errors in one file...
# http://stackoverflow.com/questions/59670/how-to-get-rid-of-deprecated-conversion-from-string-constant-to-char-warnin/541079#541079
#pragma GCC diagnostic ignored "-Wwrite-strings"


# Set Pism_EXTERNAL_LIBS and include directories.
pism_set_dependencies()

message("XXX0 " ${PROJECT_SOURCE_DIR}/slib)
message("XXX1 " ${GMP_INCLUDE_DIR})
message("XXX2 " ${CGAL_INCLUDE_DIR})
message("XXX3 " ${BLITZ++_INCLUDE_DIR})
message("XXX4 " ${Boost_INCLUDE_DIRS})
message("XXX7 " ${BOOST_ROOT})
message("XXX8 " ${PETSC_INCLUDES})
message("XXX9 " ${NETCDF_INCLUDES})

message("PROJECT_SOURCE_DIR " ${PROJECT_SOURCE_DIR})

glint2_set_dependencies()

#	${NETCDF_LIBRARIES} ${NETCDF_LIBRARIES_CXX} ${NETCDF_LIBRARIES_F77}

# -----------------------------------------------------------


# Create a library called "Hello" which includes the source file "hello.cxx".
# The extension is already found. Any number of sources could be listed here.

set(glint2_SOURCES 
	galahad/qpt_c.cpp
	galahad/zd11_c.cpp
	giss/IndexTranslator.cpp
	giss/IndexTranslator2.cpp
	giss/Proj2.cpp
	giss/SparseMatrix.cpp
	giss/geodesy.cpp
	giss/ncutil.cpp
	giss/sparsemult.cpp
	glint2/ExchangeGrid.cpp
	glint2/GCMCoupler.cpp
	glint2/Grid.cpp
	glint2/GridDomain.cpp
	glint2/GridDomain_Listed.cpp
	glint2/Grid_LonLat.cpp
	glint2/Grid_XY.cpp
	glint2/HCIndex.cpp
	glint2/HeightClassifier.cpp
	glint2/IceModel.cpp
	glint2/IceModel_DISMAL.cpp
	glint2/IceModel_Decode.cpp
	glint2/IceSheet.cpp
	glint2/IceSheet_L0.cpp
	glint2/MatrixMaker.cpp
	glint2/clippers.cpp
	glint2/gridutil.cpp
	glint2/matrix_ops.cpp
	glint2/modele/glint2_modele.cpp
	glint2/modele/grids_ll.cpp
	glint2/pism/IceModel_PISM.cpp
	glint2/pism/PISMIceModel.cpp
	glint2/pism/PSConstantGLINT2.cpp
	glint2/read_grid.cpp
	glint2/read_icemodel.cpp
	glint2/read_matrixmaker.cpp
	giss/f90blitz_f.f90
	glint2/modele/glint2_modele_f.f90
	# GALAHAD sources: turn off if USE_GALAHAD
	galahad/eqp_f.f90
	galahad/ncutil_f.f90
	galahad/qpt_f.f90
	galahad/x_loc_f.f90
	galahad/zd11_f.f90
	galahad/zd11_c.cpp
	galahad/qpt_c.cpp)

add_library (glint2 SHARED ${glint2_SOURCES})
target_link_libraries(glint2 ${Glint2_EXTERNAL_LIBS})


install(TARGETS glint2 DESTINATION lib)

# install Glint2 headers
file(GLOB_RECURSE glint2_HEADERS "*.hpp")
install(FILES ${glint2_HEADERS} DESTINATION include)
# (Consider using "install(DIRECTORY ...)" if you want to preserve
# directory structure.)


# Set RPATH in the installed library
# http://www.cmake.org/pipermail/cmake/2010-February/035157.html
set_target_properties(glint2 PROPERTIES
	INSTALL_RPATH_USE_LINK_PATH TRUE)

# ---------------------------------------------

