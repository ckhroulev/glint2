#include <mpi.h>	// For Intel MPI, mpi.h must be included before stdio.h
#include <PISMStressBalance.hh>
#include <glint2/pism/IceModel_PISM.hpp>
#include <giss/ncutil.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <cmath>
#include <sstream>
#include <string>

namespace glint2 {
namespace pism {

/** Query all the ice models to figure out what fields they need */
void IceModel_PISM::get_required_fields(std::set<IceField> &fields)
{
	fields.insert(IceField::MASS_FLUX);
//	fields.insert(IceField::ENERGY_FLUX);
//	fields.insert(IceField::SURFACE_T);
	fields.insert(IceField::TG2);
}


int IceModel_PISM::process_options()
{
	PetscErrorCode ierr;

	// Look up relevant command line options
    ierr = verbosityLevelFromOptions(); CHKERRQ(ierr);
    ierr = verbPrintf(2,pism_comm, "PISMR %s (basic evolution run mode)\n",
		      PISM_Revision); CHKERRQ(ierr);
    ierr = stop_on_version_option(); CHKERRQ(ierr);
    ierr = check_old_option_and_stop(pism_comm, "-boot_from", "-boot_file"); CHKERRQ(ierr); 

    bool iset, bfset;
    ierr = PISMOptionsIsSet("-i", iset); CHKERRQ(ierr);
    ierr = PISMOptionsIsSet("-boot_file", bfset); CHKERRQ(ierr);
    std::string usage =
      "  pismr {-i IN.nc|-boot_file IN.nc} [OTHER PISM & PETSc OPTIONS]\n"
      "where:\n"
      "  -i          IN.nc is input file in NetCDF format: contains PISM-written model state\n"
      "  -boot_file  IN.nc is input file in NetCDF format: contains a few fields, from which\n"
      "              heuristics will build initial model state\n"
      "notes:\n"
      "  * one of -i or -boot_file is required\n"
      "  * if -boot_file is used then also '-Mx A -My B -Mz C -Lz D' are required\n";
    if ((iset == PETSC_FALSE) && (bfset == PETSC_FALSE)) {
      ierr = PetscPrintf(pism_comm,
         "\nPISM ERROR: one of options -i,-boot_file is required\n\n"); CHKERRQ(ierr);
      ierr = show_usage_and_quit(pism_comm, "pismr", usage); CHKERRQ(ierr);
    } else {
      std::vector<std::string> required;  required.clear();
      ierr = show_usage_check_req_opts(pism_comm, "pismr", required, usage.c_str()); CHKERRQ(ierr);
    }
	return 0;
}

/** Initialize any grid information, etc. from the IceSheet struct.
@param vname_base Construct variable name from this, out of which to pull parameters from netCDF */
void IceModel_PISM::init(
	IceModel::GCMParams const &_gcm_params,
	std::shared_ptr<glint2::Grid> const &grid2,
	NcFile &nc,
	std::string const &vname_base,
	NcVar *const_var)
{
	printf("BEGIN IceModel_PISM::init(%s)\n", vname_base.c_str());
	IceModel_Decode::init(_gcm_params, grid2->ndata());

	dismal.reset(new IceModel_DISMAL());
	dismal->init(_gcm_params, grid2, nc, vname_base, const_var);

	std::shared_ptr<Grid_XY const> grid2_xy = std::dynamic_pointer_cast<Grid_XY const>(grid2);
	auto pism_var = nc.get_var((vname_base + ".pism").c_str());	// PISM parameters
	if (allocate(grid2_xy, pism_var, const_var) != 0) {
		PetscPrintf(gcm_params.gcm_comm, "IceModel_PISM::IceModel_PISM(...): allocate() failed\n");
		PISMEnd();
	}
	printf("END IceModel_PISM::init()\n");
}

void IceModel_PISM::update_ice_sheet(
	NcFile &nc,
	std::string const &vname,
	IceSheet *sheet)
{
	printf("BEGIN IceModel_PISM::update_ice_sheet(%s)\n", vname.c_str());

	auto pism_var = nc.get_var((vname + ".pism").c_str());	// PISM parameters
	auto pism_i_att(giss::get_att(pism_var, "i"));	// PISM -i argument (input file)
	std::string pism_i = boost::filesystem::absolute(boost::filesystem::path(
		pism_i_att->as_string(0)), gcm_params.config_dir).string();

	// Read variables from PISM input file
	// byte mask(time, x, y) ;
	// 		mask:units = "" ;
	// 		mask:coordinates = "lat lon" ;
	// 		mask:flag_meanings = "ice_free_bedrock grounded_ice floating_ice ice_free_ocean" ;
	// 		mask:grid_mapping = "mapping" ;
	// 		mask:long_name = "ice-type (ice-free/grounded/floating/ocean) integer mask" ;
	// 		mask:pism_intent = "diagnostic" ;
	// 		mask:flag_values = 0b, 2b, 3b, 4b ;
	// double thk(time, x, y) ;
	// 		thk:units = "m" ;
	// 		thk:valid_min = 0. ;
	// 		thk:coordinates = "lat lon" ;
	// 		thk:grid_mapping = "mapping" ;
	// 		thk:long_name = "land ice thickness" ;
	// 		thk:pism_intent = "model_state" ;
	// 		thk:standard_name = "land_ice_thickness" ;
	// double topg(time, x, y) ;
	// 		topg:units = "m" ;
	// 		topg:coordinates = "lat lon" ;
	// 		topg:grid_mapping = "mapping" ;
	// 		topg:long_name = "bedrock surface elevation" ;
	// 		topg:pism_intent = "model_state" ;
	// 		topg:standard_name = "bedrock_altitude" ;
printf("Opening PISM file for elev2 and mask2: %s\n", pism_i.c_str());
	NcFile ncin(pism_i.c_str());
	long ntime = ncin.get_dim("time")->size();
	long nx = ncin.get_dim("x")->size();
	long ny = ncin.get_dim("y")->size();

	blitz::Array<ncbyte,2> mask(nx, ny);
	NcVar *mask_var = ncin.get_var("mask");
	mask_var->set_cur(ntime-1, 0, 0);
	mask_var->get(mask.data(), 1, nx, ny);

	NcVar *thk_var = ncin.get_var("thk");
	blitz::Array<double,2> thk(nx, ny);
	thk_var->set_cur(ntime-1, 0, 0);
	thk_var->get(thk.data(), 1, nx, ny);

	NcVar *topg_var = ncin.get_var("topg");
	blitz::Array<double,2> topg(nx, ny);
	topg_var->set_cur(ntime-1, 0, 0);
	topg_var->get(topg.data(), 1, nx, ny);

	ncin.close();

	// Transpose and copy the data
	if (!sheet->mask2.get()) sheet->mask2.reset(
		new blitz::Array<int,1>(glint2_grid->ndata()));
	for (int i=0; i<nx; ++i) {
	for (int j=0; j<ny; ++j) {
		int ix2 = glint2_grid->ij_to_index(i, j);
		sheet->elev2(ix2) = topg(i,j) + thk(i,j);
		// Mask uses same convention as MATPLOTLIB: 1 = masked out
		(*sheet->mask2)(ix2) = (mask(i,j) == 2 ? 0 : 1);
	}}

	printf("END IceModel_PISM::update_ice_sheet()\n");
}



IceModel_PISM::~IceModel_PISM()
{
	if (deallocate() != 0) {
		PetscPrintf(pism_comm, "IceModel_PISM::IceModel_PISM(...): allocate() failed\n");
		PISMEnd();
	}
}

// See: http://stackoverflow.com/questions/3418231/replace-part-of-a-string-with-another-string
bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


// Arguments that are paths, and thus need pathname resolution
// For stable0.5 branch
// static std::set<std::string> path_args = {"config_override", "i", "o", "surface_given_file", "extra_file", "ts_file"};
// For dev branch
static std::set<std::string> path_args = {"i", "o", "surface_given_file", "ocean_kill", "extra_file", "ts_file"};

PetscErrorCode IceModel_PISM::allocate(
	std::shared_ptr<const glint2::Grid_XY> &glint2_grid,
	NcVar *pism_var, NcVar *const_var)
{
	this->glint2_grid = glint2_grid;

	// Create arguments from PISM configuration
	std::vector<std::string> args;
	args.push_back("glint2_pism");

	// Get arguments from the GCM
	args.push_back("-reference_date");
	giss::time::tm const &tb(gcm_params.time_base);
	args.push_back((boost::format("%04d-%02d-%02d") % tb.year() % tb.month() % tb.mday()).str());

	// Get arguments from GLINT2 configuration
	for (int i=0; i<pism_var->num_atts(); ++i) {
		auto att = giss::get_att(pism_var, i);
		std::string name = att->name();
		std::string val = att->as_string(0);

		if (path_args.find(name) == path_args.end()) {
			// Regular case, just use the value there
			val = std::string(att->as_string(0));
		} else {
			// Resolve path names according to the configuration directory
			val = boost::filesystem::absolute(
				boost::filesystem::path(att->as_string(0)),
				gcm_params.config_dir).string();
			printf("IceModel_PISM resolving %s: %s --> %s\n", name.c_str(), att->as_string(0), val.c_str());
		}

		args.push_back("-" + name);
		args.push_back(val);
	}

	// Convert those arguments to old C style
	int argc = args.size();
	char *argv_array[argc];
	std::vector<char> all_str;
	for (int i=0; i<argc; ++i) {
		std::string &arg = args[i];
		for (unsigned int j=0; j<arg.size(); ++j) all_str.push_back(arg[j]);
		all_str.push_back('\0');
	}
	char *pos = &all_str[0];
	for (int i=0; i<argc; ++i) {
		std::string &arg = args[i];
		argv_array[i] = pos;
		pos +=arg.size() + 1;
	}
	char **argv = argv_array;

printf("*** PISM Args:");
for (int i=0; i<argc; ++i) printf(" %s", argv[i]);
printf("\n");


	// Set up communicator for PISM to use
	// Use same group of processes.
	// No spawning or intercommunicators for now --- maybe not ever.
//	MPI_Comm_dup(gcm_params.gcm_comm, &pism_comm);
	pism_comm = gcm_params.gcm_comm;
	PetscErrorCode ierr;
	ierr = MPI_Comm_rank(pism_comm, &pism_rank); CHKERRQ(ierr);
	ierr = MPI_Comm_size(pism_comm, &pism_size); CHKERRQ(ierr);

printf("[%d] pism_size = %d\n", pism_rank, pism_size);

	// Initialize Petsc
printf("Initializing PETSc\n");
	petsc_context.reset(new PetscContext(pism_comm, argc, argv));

    unit_system.reset(new ::PISMUnitSystem(NULL));
    config.reset(new ::PISMConfig(pism_comm, "pism_config", *unit_system));
	overrides.reset(new ::PISMConfig(pism_comm, "pism_overrides", *unit_system));
    ierr = init_config(pism_comm, *config, *overrides, true); CHKERRQ(ierr);

//#if 0
//	if (pism_rank == 0) {
		auto full_fname(gcm_params.config_dir / "glint2_pism_config.nc");
		printf("IceModel_PISM writing config (1) to: %s\n", full_fname.c_str());
		config->write(full_fname.c_str());
//	}
//#endif

    pism_grid.reset(new ::IceGrid(pism_comm, *config));
printf("pism_grid=%p: (xs,xm,ys,ym,Mx,My) = %d %d %d %d %d %d %ld %ld\n", &*pism_grid, pism_grid->xs, pism_grid->xm, pism_grid->ys, pism_grid->ym, pism_grid->Mx, pism_grid->My, pism_grid->x.size(), pism_grid->y.size());
    ice_model.reset(new PISMIceModel(*pism_grid, *config, *overrides));

	// Transfer constants from GLINT2 to PISM
	double ice_density = giss::get_att(const_var, "ice_density")->as_double(0);
	config->set_double("ice_density", ice_density);
	BY_ICE_DENSITY = 1.0d / ice_density;
	config->set_double("ice_specific_heat_cpacity", giss::get_att(const_var, "ice_specific_heat_capacity")->as_double(0));
	config->set_double("ideal_gas_constant", giss::get_att(const_var, "ideal_gas_constant")->as_double(0));


printf("[%d] start = %f\n", pism_rank, pism_grid->time->start());
printf("[%d] end = %f\n", pism_rank, pism_grid->time->end());

    ierr = ice_model->setExecName("pismr"); CHKERRQ(ierr);
	// This has the following stack trace:
	// 	IceModel::init()					[iceModel.cc]
	// 	IceModel::model_state_setup()		[iMinit.cc]
	// 	IceModel::init_couplers()			[iMinit.cc]
	// 	surface->init()
	// We need to have called attach_surface_model() by now, so that
	// pism_surface_model->init() will be called.
printf("[%d] Before ice_model->init()\n", pism_rank);
    ierr = ice_model->init(); CHKERRQ(ierr);
printf("[%d] After ice_model->init()\n", pism_rank);
    ierr = ice_model->init_run(); CHKERRQ(ierr);

//	// Add our stock initialization to it
//	pism_surface_model.reset(new PSConstantGLINT2(*pism_grid, *config));
//	ice_model->attach_surface_model(pism_surface_model.get());

	// Fetch out our pism_surface_model
	pism_surface_model = ice_model->ps_constant_glint2();
printf("pism_surface_model = %p\n", pism_surface_model);

	// Set up corresponence between GLINT2 fields and variables
	// in the PISM data structures.
	pism_vars.insert(std::make_pair(IceField::MASS_FLUX,
		&pism_surface_model->climatic_mass_balance));
	pism_vars.insert(std::make_pair(IceField::TG2,
		&pism_surface_model->ice_surface_temp));

	// Initialize scatter/gather stuff
printf("pism_grid->max_stencil_width = %d\n", pism_grid->max_stencil_width);
	ierr = pism_grid->get_dm(1, pism_grid->max_stencil_width, da2); CHKERRQ(ierr);

	ierr = DMCreateGlobalVector(da2, &g2); CHKERRQ(ierr);


	// note we want a global Vec but reordered in the natural ordering
	// so when it is scattered to proc zero it is not all messed up;
	// see above
	ierr = DMDACreateNaturalVector(da2, &g2natural); CHKERRQ(ierr);

	// next get context *and* allocate samplep0 (on proc zero only, naturally)
	ierr = VecScatterCreateToZero(g2natural, &scatter, &Hp0); CHKERRQ(ierr);

	// Check that grid dimensions match
printf("IceModel_PISM checking grid dimensions: pism=(%d, %d) glint2=(%d, %d)\n", pism_grid->Mx, pism_grid->My, glint2_grid->nx(), glint2_grid->ny());
	if ((pism_grid->Mx != glint2_grid->nx()) || (pism_grid->My != glint2_grid->ny())) {
		fprintf(stderr, "Grid mismatch: pism=(%d, %d) glint2=(%d, %d)\n", pism_grid->Mx, pism_grid->My, glint2_grid->nx(), glint2_grid->ny());
		throw std::exception();
	}

	// Initialize variables to store 2D sums of 3D things
	// ice upper surface elevation
	ierr = strain_heating2.create(*pism_grid, "strain_heating2", WITHOUT_GHOSTS); CHKERRQ(ierr);
	ierr = strain_heating2.set_attrs("internal",
		"rate of strain heating in ice (dissipation heating), summed over column",
		"W m-2", ""); CHKERRQ(ierr);


	return 0;
}

PetscErrorCode IceModel_PISM::deallocate()
{
	PetscErrorCode ierr;

	ierr = VecDestroy(&g2); CHKERRQ(ierr);
	ierr = VecDestroy(&g2natural); CHKERRQ(ierr);
	// ierr = VecScatterDestroy(&scatter); CHKERRQ(ierr);
//	ierr = VecDestroy(&Hp0); CHKERRQ(ierr);

	return 0;
}


// --------------------------------------------------------
/** Sum a 3-D vector in the Z direction to create a 2-D vector.

<p>Note that this sums up all the values in a column, including ones
above the ice. This may or may not be what you need. Also, take a look
at IceModel::compute_ice_enthalpy(PetscScalar &result) in iMreport.cc.</p>

<p>As for the difference between IceModelVec2 and IceModelVec2S, the
former can store fields with more than 1 "degree of freedom" per grid
point (such as 2D fields on the "staggered" grid, with the first
degree of freedom corresponding to the i-offset and second to
j-offset).</p>

<p>IceModelVec2S is just IceModelVec2 with "dof == 1", and
IceModelVec2V is IceModelVec2 with "dof == 2". (Plus some extra
methods, of course.)</p>

<p>Either one of IceModelVec2 and IceModelVec2S would work in this
case.</p>

@see https://github.com/pism/pism/issues/229 */
static int sum_columns(IceModelVec3 &input, IceModelVec2S &output)
{
  PetscScalar *column = NULL;
  IceGrid &grid = *input.get_grid();
  int ierr = 0;

  ierr = input.begin_access(); CHKERRQ(ierr);
  ierr = output.begin_access(); CHKERRQ(ierr);
  for (PetscInt   i = grid.xs; i < grid.xs+grid.xm; ++i) {
    for (PetscInt j = grid.ys; j < grid.ys+grid.ym; ++j) {
      ierr = input.getInternalColumn(i, j, &column); CHKERRQ(ierr);

      output(i,j) = 0.0;
      for (unsigned int k = 0; k < grid.Mz; ++k)
        output(i,j) += column[k];
    }
  }
  ierr = output.end_access(); CHKERRQ(ierr);
  ierr = input.end_access(); CHKERRQ(ierr);

  return 0;
}

// --------------------------------------------------
/** glint2_var Variable, already allocated, to receive data
@param glint2_var_xy The array to write into.  If this array is not yet allocated,
it will be allocated.*/
PetscErrorCode IceModel_PISM::iceModelVec2S_to_blitz_xy(IceModelVec2S &pism_var, blitz::Array<double,2> &ret)
{
	PetscErrorCode ierr;
	Vec g;

printf("iceModelVec2S_to_blitz_xy:\n");
printf("nx() ny() = %d, %d\n", nx(), ny());
printf("Mx My = %d, %d\n", pism_grid->Mx, pism_grid->My);

	auto xy_shape(blitz::shape(ny(), nx()));
	if (ret.size() == 0) {
		ret.reference(blitz::Array<double,2>(ny(), nx()));
	} else {
		if (ret.extent(0) != xy_shape[0] || ret.extent(1) != xy_shape[1]) {
			fprintf(stderr, "IceModel_PISM::iceModelVec2S_to_blitz_xy(): ret(%d, %d) should be (%d, %d)\n", ret.extent(0), ret.extent(1), xy_shape[0], xy_shape[1]);
			throw std::exception();
		}
	}

	if (pism_var.get_dof() != 1)
		SETERRQ(pism_grid->com, 1, "This method only supports IceModelVecs with dof == 1");

	// Gather data to one processor
	PetscScalar **bHp0;
	ierr = pism_var.put_on_proc0(Hp0, scatter, g2, g2natural); CHKERRQ(ierr);

	// Copy it to blitz array
	ierr = VecGetArray2d(Hp0, pism_grid->Mx, pism_grid->My, 0, 0, &bHp0);
	for (PetscInt i=0; i < pism_grid->Mx; i++) {
		for (PetscInt j=0; j < pism_grid->My; j++) {
			ret(j, i) = bHp0[i][j];
		}
	}
	ierr = VecRestoreArray2d(Hp0, pism_grid->Mx, pism_grid->My, 0, 0, &bHp0);
}
// --------------------------------------------------
void IceModel_PISM::run_decoded(double time_s,
	std::map<IceField, blitz::Array<double,1>> const &vals2)
{
	dismal->run_decoded(time_s, vals2);

	if (run_decoded_petsc(time_s, vals2) != 0) {
		PetscPrintf(pism_comm, "IceModel_PISM::runtimestep() failed\n");
		PISMEnd();
	}
}

// --------------------------------------------------

PetscErrorCode IceModel_PISM::run_decoded_petsc(double time_s,
	std::map<IceField, blitz::Array<double,1>> const &vals2)
{
printf("%d IceModel_PISM::run_decoded_petsc(%f)\n", pism_rank, time_s);
	PetscErrorCode ierr;

	// Convert from GLINT2 2D indices, to PISM 2D indices.
	// (one does x-major, the other y-major ordering)
	// Also, pack those indices into a dense array
	// (the incoming blitz::Array is not dense)
	std::unique_ptr<int[]> g2_ix(new int[ndata()]);
#if 0
	for (int ix0=0; ix0<ndata(); ++ix0) {
		int ii,jj;
		glint2_grid->index_to_ij(ix0, ii, jj);
//printf("indices %d -> %d %d\n", indices(i), ii,jj);
		int ix1 = ii * glint2_grid->ny() + jj;
		g2_ix[ix0] = ix1;
	}
#endif
	// Transfer input to PISM variables (and scatter w/ PETSc as well)
	std::unique_ptr<PetscScalar[]> g2_y(new PetscScalar[ndata()]);
	for (auto ii = vals2.begin(); ii != vals2.end(); ++ii) {
		IceField field(ii->first);
		blitz::Array<double,1> const &val(ii->second);

		// Go on if PISM is not interested in this field
		auto pism_var_ii(pism_vars.find(field));
		if (pism_var_ii == pism_vars.end()) continue;

		// Densify the values array, and convert units
printf("BY_ICE_DENSITY = %f\n", BY_ICE_DENSITY);
		int nval = 0;
		switch(field.index()) {
			case IceField::MASS_FLUX : {
				// GLINT2: kg/(s m^2) --> "kg m-2 s-1" in PISM
				for (int ix0=0; ix0<ndata(); ++ix0) {
					if (std::isnan(val(ix0))) continue;

					g2_y[nval] = val(ix0);

//					int ii,jj;
//					glint2_grid->index_to_ij(ix0, ii, jj);
//					int ix1 = ii * glint2_grid->ny() + jj;
//					g2_ix[nval] = ix1;

					g2_ix[nval] = glint2_to_pism1d(ix0);
					++nval;
				}
			} break;
			case IceField::TG2 : {
				// GLINT2: C --> PISM: K
				for (int ix0=0; ix0<ndata(); ++ix0) {
					if (std::isnan(val(ix0))) {
//						g2_y[nval] = giss::C2K;
continue;		// GLINT2 and PISM land surface masks must match!
					} else {
						g2_y[nval] = val(ix0) + giss::C2K;
					}

					int ii,jj;
					glint2_grid->index_to_ij(ix0, ii, jj);
					int ix1 = ii * glint2_grid->ny() + jj;
					g2_ix[nval] = ix1;

					++nval;
				}
			} break;
		}
//for (int i=0; i<ndata(); ++i) g2_y[i] = 17.0;

		// Put into a natural-ordering global distributed Petsc Vec
		ierr = VecSet(g2natural, 0.0); CHKERRQ(ierr);
#if 1
//for (int i=0; i<ndata(); ++i) printf("G2 %s: %d %f\n", field.str(), g2_ix[i], g2_y[i]);
		ierr = VecSetValues(g2natural, nval, g2_ix.get(), g2_y.get(), INSERT_VALUES); CHKERRQ(ierr);
#else
// Debug: send in zero vectors
ierr = VecSetValues(g2natural, 0, g2_ix.get(), g2_y.get(), INSERT_VALUES); CHKERRQ(ierr);
#endif

		ierr = VecAssemblyBegin(g2natural); CHKERRQ(ierr);
		ierr = VecAssemblyEnd(g2natural); CHKERRQ(ierr);

		// Copy to Petsc-ordered global vec
		ierr = DMDANaturalToGlobalBegin(da2, g2natural, INSERT_VALUES, g2); CHKERRQ(ierr);
		ierr =   DMDANaturalToGlobalEnd(da2, g2natural, INSERT_VALUES, g2); CHKERRQ(ierr);

		// Copy to the output variable
		// (Could we just do DMDANaturalToGlobal() directly to this?)
		ierr = pism_var_ii->second->copy_from(g2); CHKERRQ(ierr);

long time_day = (int)(time_s / 86400. + .5);
std::stringstream fname;
std::string fnpart;
switch(field.index()) {
	case IceField::MASS_FLUX :
		// GLINT2: kg/(s m^2) --> m s-1 ice:
		fnpart = "climatic_mass_balance";
	break;
	case IceField::TG2 :
		// GLINT2: C --> PISM: K
		fnpart = "ice_surface_temp";
	break;
}
fname << time_day << "-" << fnpart << ".nc";
boost::filesystem::path pfname(gcm_params.config_dir / "dismal_out2" / fname.str());

printf("ICeModel_PISM writing (2) to: %s\n", pfname.c_str());
pism_var_ii->second->dump(pfname.c_str());

	}

printf("[%d] BEGIN ice_model->run_to(%f) %p\n", pism_rank, time_s, ice_model.get());
	// Run PISM for one timestep
	ice_model->run_to(time_s);

	// Retrieve stuff from PISM
	blitz::Array<double,2> geothermal_flux;
	iceModelVec2S_to_blitz_xy(*ice_model->get_geothermal_flux(), geothermal_flux);

	// Sum strain_heating over the Z direction and retrieve from PISM
	IceModelVec3 *strain_heating3p;
	ice_model->get_stress_balance()->get_volumetric_strain_heating(strain_heating3p);
	ierr = sum_columns(*strain_heating3p, strain_heating2); CHKERRQ(ierr);
	blitz::Array<double,2> strain_heating2b;	// GLINT2-style array
	iceModelVec2S_to_blitz_xy(strain_heating2, strain_heating2b);

printf("[%d] END ice_model->run_to()\n");



if (pism_rank == 0) {
	char fname[30];
	long time_day = (int)(time_s / 86400. + .5);
	sprintf(fname, "%ld-pismout.nc", time_day);
	auto full_fname(gcm_params.config_dir / "dismal_out2" / fname);
	NcFile ncout(full_fname.c_str(), NcFile::Replace);

	NcDim *ny_dim = ncout.add_dim("ny", ny());
	NcDim *nx_dim = ncout.add_dim("nx", nx());

	std::vector<boost::function<void ()>> fns;
	fns.push_back(giss::netcdf_define(ncout, "strain_heating", strain_heating2b, {ny_dim, nx_dim}));
	fns.push_back(giss::netcdf_define(ncout, "geothermal_flux", geothermal_flux, {ny_dim, nx_dim}));
	
    // Write data to netCDF file
    for (auto ii = fns.begin(); ii != fns.end(); ++ii) (*ii)();
    ncout.close();
}


	return 0;
}

}}
