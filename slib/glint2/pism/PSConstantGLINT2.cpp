// Copyright (C) 2008-2014 PISM Authors
//
// This file is part of PISM.
//
// PISM is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation; either version 3 of the License, or (at your option) any later
// version.
//
// PISM is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.	See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License
// along with PISM; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA	02110-1301	USA

#include "PSConstantGLINT2.hpp"
#include <PIO.hh>
#include <PISMVars.hh>
#include <IceGrid.hh>

namespace glint2 {
namespace pism {

PSConstantGLINT2::PSConstantGLINT2(IceGrid &g, const ::PISMConfig &conf)
	: PISMSurfaceModel(g, conf)
{
	PetscErrorCode ierr = allocate_PSConstantGLINT2(); CHKERRCONTINUE(ierr);
	if (ierr != 0) {
		PISMEnd();
	}
}

void PSConstantGLINT2::attach_atmosphere_model(PISMAtmosphereModel *input)
{
	delete input;
}

PetscErrorCode PSConstantGLINT2::allocate_PSConstantGLINT2()
{
printf("BEGIN PSConstantGLINT2::allocate_PSConstantGLINT2()\n");
	PetscErrorCode ierr;

printf("PSConstantGLINT2::allocate(): grid=%p, Mx My = %d %d\n", &grid, grid.Mx, grid.My);
	ierr = climatic_mass_balance.create(grid, "climatic_mass_balance", WITHOUT_GHOSTS); CHKERRQ(ierr);
	ierr = climatic_mass_balance.set_attrs("climate_state",
		"constant-in-time ice-equivalent surface mass balance (accumulation/ablation) rate",
		"kg m-2 s-1",
		"land_ice_surface_specific_mass_balance"); CHKERRQ(ierr);
	ierr = climatic_mass_balance.set_glaciological_units("kg m-2 year-1"); CHKERRQ(ierr);
	climatic_mass_balance.write_in_glaciological_units = true;

	ierr = ice_surface_temp.create(grid, "ice_surface_temp", WITHOUT_GHOSTS); CHKERRQ(ierr);
	ierr = ice_surface_temp.set_attrs("climate_state",
		"constant-in-time ice temperature at the ice surface",
		"K", ""); CHKERRQ(ierr);
printf("END PSConstantGLINT2::allocate_PSConstantGLINT2()\n");
	return 0;
}

PetscErrorCode PSConstantGLINT2::init(PISMVars &vars)
{
printf("BEGIN PSConstantGLINT2::init()\n");
	PetscErrorCode ierr;
	bool do_regrid = false;
	int start = -1;

	m_t = m_dt = GSL_NAN;	// every re-init restarts the clock

	ierr = verbPrintf(2, grid.com,
		 "* Initializing the PSConstantGLINT2 surface model. Serves as storage for climate fields.\n"
		 "	Any choice of atmosphere coupler (option '-atmosphere') is ignored.\n"); CHKERRQ(ierr);

	// find PISM input file to read data from:
	ierr = find_pism_input(input_file, do_regrid, start); CHKERRQ(ierr);

	// read snow precipitation rate from file
	ierr = verbPrintf(2, grid.com,
		"		reading ice-equivalent surface mass balance rate 'climatic_mass_balance' from %s ... \n",
		input_file.c_str()); CHKERRQ(ierr);
	if (do_regrid) {
		ierr = climatic_mass_balance.regrid(input_file, CRITICAL); CHKERRQ(ierr); // fails if not found!
	} else {
		ierr = climatic_mass_balance.read(input_file, start); CHKERRQ(ierr); // fails if not found!
	}

	// Set ice_surface_temp to a harmless value for now. (FIXME, though.)
	ierr = ice_surface_temp.set(grid.convert(-10.0, "Celsius", "Kelvin")); CHKERRQ(ierr);

	// parameterizing the ice surface temperature 'ice_surface_temp'
	ierr = verbPrintf(2, grid.com,
				"		parameterizing the ice surface temperature 'ice_surface_temp' ... \n"); CHKERRQ(ierr);

printf("END PSConstantGLINT2::init()\n");
	return 0;
}

PetscErrorCode PSConstantGLINT2::update(PetscReal my_t, PetscReal my_dt)
{
	PetscErrorCode ierr;

printf("BEGIN update(%f, %f)\n", my_t, my_dt);

	if ((fabs(my_t - m_t) < 1e-12) &&
			(fabs(my_dt - m_dt) < 1e-12))
		return 0;

	m_t	= my_t;
	m_dt = my_dt;

printf("PSConstantGLINT2::update(%f) dumping variables\n", my_t);
ice_surface_temp.dump("ice_surface_temp.nc");
climatic_mass_balance.dump("climatic_mass_balance.nc");
printf("PSConstantGLINT2::update(%f) done dumping variables\n", my_t);

	return 0;
}

void PSConstantGLINT2::get_diagnostics(std::map<std::string, PISMDiagnostic*> &/*dict*/,
	std::map<std::string, PISMTSDiagnostic*> &/*ts_dict*/)
{
	// empty (does not have an atmosphere model)
}

PetscErrorCode PSConstantGLINT2::ice_surface_mass_flux(IceModelVec2S &result) {
	PetscErrorCode ierr;

	ierr = climatic_mass_balance.copy_to(result); CHKERRQ(ierr);

	return 0;
}

PetscErrorCode PSConstantGLINT2::ice_surface_temperature(IceModelVec2S &result) {
	PetscErrorCode ierr;

	ierr = ice_surface_temp.copy_to(result); CHKERRQ(ierr);

	return 0;
}

void PSConstantGLINT2::add_vars_to_output(std::string /*keyword*/, std::set<std::string> &result) {
	result.insert("climatic_mass_balance");
	result.insert("ice_surface_temp");
	// does not call atmosphere->add_vars_to_output().
}

PetscErrorCode PSConstantGLINT2::define_variables(std::set<std::string> vars, const PIO &nc, PISM_IO_Type nctype) {
	PetscErrorCode ierr;

	ierr = PISMSurfaceModel::define_variables(vars, nc, nctype); CHKERRQ(ierr);

	if (set_contains(vars, "ice_surface_temp")) {
		ierr = ice_surface_temp.define(nc, nctype); CHKERRQ(ierr);
	}

	if (set_contains(vars, "climatic_mass_balance")) {
		ierr = climatic_mass_balance.define(nc, nctype); CHKERRQ(ierr);
	}

	return 0;
}

PetscErrorCode PSConstantGLINT2::write_variables(std::set<std::string> vars, const PIO &nc) {
	PetscErrorCode ierr;

	if (set_contains(vars, "ice_surface_temp")) {
		ierr = ice_surface_temp.write(nc); CHKERRQ(ierr);
	}

	if (set_contains(vars, "climatic_mass_balance")) {
		ierr = climatic_mass_balance.write(nc); CHKERRQ(ierr);
	}

	return 0;
}

}		// namespace glint2::pism
}		// namespace glint2
