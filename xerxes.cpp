#include <vector>
#include <cstdio>
#include <cstring>
#include <chrono>

#include <madness/mra/mra.h>

#include "density_projector3d.hpp"

using namespace madness;

typedef double real_t;
typedef Vector<double,3> coordT;

extern "C" void get_dim_(int* nx, int* ny, int* nz, int* nparticles);
extern "C" void part_init_(const int* nx, const int* ny, const int* nz, const int* nparticles, real_t* x, real_t* y, real_t* z, real_t* vx, real_t* vy, real_t* vz, real_t* mass);
extern "C" void project_density_(const int* nx, const int* ny, const int* nz, const int* nparticles, const real_t* x, const real_t* y, const real_t* z, real_t* mass, real_t* density, const int* step);

void set_initial_parameters(const int& nx){
	
	BoundaryConditions<3> bc(BC_PERIODIC);
	
	FunctionDefaults<3>::set_cubic_cell((double) 1, (double) nx);
	FunctionDefaults<3>::set_bc(bc); 
	FunctionDefaults<3>::set_apply_randomize(true);
	FunctionDefaults<3>::set_autorefine(true);
	FunctionDefaults<3>::set_refine(true);
	
}

void set_projection_precision(const int& order, const double& threshold){
	
	FunctionDefaults<3>::set_k(order);
	FunctionDefaults<3>::set_thresh(threshold);
	
}

void build_projected_density(World& world, const int& nx, const int& ny, const int& nz, real_t* density, real_function_3d& projected_density){
	
	real_functor_3d density_functor;
	
	density_functor = real_functor_3d(new DensityProjector(nx, ny, nz, &density[0]));
	projected_density = real_factory_3d(world).functor(density_functor);
	
}

void compute_potential(World& world, const real_function_3d& projected_density, real_function_3d& potential, const double& precision, const double& threshold){
	
	double integral, volume, mean;

	// if (world.rank() == 0) printf("\tProjecting potential\n");
	
	real_convolution_3d coulomb_operator = CoulombOperator(world, precision, threshold);
	
	potential = coulomb_operator(projected_density);

	
	// if (world.rank() == 0) printf("\tProjected potential\n");

	integral = (potential).trace();
	volume = FunctionDefaults<3>::get_cell_volume();
	mean = integral/volume;

	potential = potential - mean;
	// if (world.rank() == 0) printf("\tNormalized\n");
	
	// if (world.rank() == 0) printf("\t#YOLO FTW");
	// real_derivative_3d Dx(world, 0), Dy(world, 1), Dz(world, 2);
	// real_function_3d deriv_x = Dx(*potential);
	
}

void print_density(World& world, const real_function_3d& projected_density, const int& numpts, const int& nx){
	
	const char filename_density[] = "data/spartan_density.vts";
	
	Vector<double, 3> plotlo, plothi;
	Vector<long, 3> npoints;
	
	
	for(int i(0); i < 3; ++i){
		plotlo[i] = 1;
		plothi[i] = (double) nx;
		npoints[i] = numpts;
	}
	
	plotvtk_begin(world, filename_density, plotlo, plothi, npoints);
	plotvtk_data(projected_density, "density", world, filename_density, plotlo, plothi, npoints);
	plotvtk_end<3>(world, filename_density);
	
	// if (world.rank() == 0) printf("Printed...\n\n");
	
}

void print_potential(World& world, const real_function_3d& potential, const int& numpts, const int& nx){
	
	const char filename_potential[] = "data/spartan_potential.vts";
	
	Vector<double, 3> plotlo, plothi;
	Vector<long, 3> npoints;
	
	
	for(int i(0); i < 3; ++i){
		plotlo[i] = 1;
		plothi[i] = (double) nx;
		npoints[i] = numpts;
	}
	
	plotvtk_begin(world, filename_potential, plotlo, plothi, npoints);
	plotvtk_data(potential, "potential", world, filename_potential, plotlo, plothi, npoints);
	plotvtk_end<3>(world, filename_potential);
	
}

real_function_3d solve_potential(World& world, real_t* x, real_t* y, real_t* z, const int& nx, const int& ny, const int& nz, const int& nparticles, real_t* density){
	
	real_function_3d rho_interp;
	real_function_3d phi;
	coord_3d center;
	real_function_3d temp;
	int limit;
	
	// if (world.rank() == 0) printf("Setup initial parameters\n");
	set_initial_parameters(nx);
	// if (world.rank() == 0) printf("Set...\n\n");
	
	// if (world.rank() == 0) printf("Setup projection precision\n");
	set_projection_precision(9, 1e-7);
	// if (world.rank() == 0) printf("Set...\n\n");
	
	// if (world.rank() == 0) printf("Build projected density\n");
	build_projected_density(world, nx, ny, nz, density, rho_interp);
	// if (world.rank() == 0) printf("Built...\n\n");
	
	
	// if (world.rank() == 0) printf("\tPrinting density\n");
	// print_density(world, rho_interp, 128, nx);
	// if (world.rank() == 0) printf("\tPrinted...\n\n");

	// if (world.rank() == 0) printf("Computing potential\n");
	
	compute_potential(world, rho_interp, phi, 1e-6, 1e-8);
	
	// //
	// // potential = &phi;
	//
	// compute_potential(world, rho_interp, potential, 1e-6, 1e-8);
	// if (world.rank() == 0) printf("Computed...\n\n");
	
	//
	// double temp;
	// temp = phi(5.0, 5.0, 5.0);
	// if (world.rank() == 0) printf("Eval potential %f\n", temp);
	
	// if (world.rank() == 0) printf("Printing potential\n");
	// print_potential(world, potential, 128, nx);
	// if (world.rank() == 0) printf("Printed...\n\n");	
	
	return phi;
}

void compute_gradient(World& world, const real_function_3d& potential, vector_real_function_3d& gradient){
	
	real_derivative_3d Dx(world, 0), Dy(world, 1), Dz(world, 2);
	
	gradient[0] = Dx(potential);
	gradient[1] = Dy(potential);
	gradient[2] = Dz(potential);

}


// static void update_velocity(const coordT& position, coordT& velocity, const real_t& time_step, vector_real_function_3d& gradient){
//
// 	for(int direction(0); direction < 3; ++direction){
// 		velocity[direction] += gradient[direction].eval(position) * time_step;
// 	}
//
// }
//
// static void update_position(coordT& position, const coordT& velocity, const real_t& time_step){
//
// 	for(int axis(0); axis < 3; ++axis){
// 		position[axis] += velocity[axis] * time_step;
// 	}
//
// }


void update_particles(World& world, real_t* x, real_t* y, real_t* z, real_t* vx, real_t* vy, real_t* vz, const int& nparticles, const real_function_3d& potential, const real_t& timestep){
	
	const int nx(128), ny(128), nz(128);
	
	vector_real_function_3d gradient(3);
	const int upper_limit = nparticles;
	
	compute_gradient(world, potential, gradient);
		
	for(int particle = world.rank(); particle < upper_limit; particle += world.size()){

		// Is it really useful to create a coordT like this, for nothing? I don't think so...
		coordT position, velocity;
		position[0] = x[particle]; position[1] = y[particle]; position[2] = z[particle];
		velocity[0] = vx[particle]; velocity[1] = vy[particle]; velocity[2] = vz[particle];

		// update_velocity(position, velocity, timestep, gradient);
		//
		// update_position(position, velocity, timestep);
		
		for(int direction(0); direction < 3; ++direction){
			
			// Issue is here, probably doesn't like the accessing of gradient
			velocity[direction] += gradient[direction].eval(position) * timestep;
			// position[direction] += velocity[direction] * timestep;
		}
		
		for(int direction(0); direction < 3; ++direction){
			// Issue is here, probably doesn't like the accessing of gradient
			// velocity[direction] += gradient[direction].eval(position) * timestep;
			position[direction] += velocity[direction] * timestep;
		}
		//
		// Switch to local variable
		
		x[particle] = position[0] + ( position[0] > nx ? -(nx-1) : (position[0] < 1 ? (nx-1) : 0) ); 
		y[particle] = position[1] + ( position[1] > ny ? -(ny-1) : (position[1] < 1 ? (ny-1) : 0) ); 
		z[particle] = position[2] + ( position[2] > nz ? -(nz-1) : (position[2] < 1 ? (nz-1) : 0) );
		
		vx[particle] = velocity[0]; vy[particle] = velocity[1]; vz[particle] = velocity[2];

	}
	
	world.gop.fence();
	
	// if (world.rank() == 0) printf("\tDone.\n\n");
	// if (world.rank() == 0) printf("\tUpdate time was %f.\n", update_time);
		
}

int main(int argc, char** argv){
	
	int nx, ny, nz, nparticles;
	int nstep;
	std::vector<real_t> x, y, z, vx, vy, vz, mass, density;
	double timestep;
	
	// Put algorithm to do adaptive timestepping
	timestep = 5;
	
	initialize(argc, argv);
	World world(SafeMPI::COMM_WORLD);
	startup(world, argc, argv);

	get_dim_(&nx, &ny, &nz, &nparticles);

	x.resize(nparticles);
	y.resize(nparticles);
	z.resize(nparticles);
	vx.resize(nparticles);
	vy.resize(nparticles);
	vz.resize(nparticles);
	mass.resize(nparticles);
	density.resize(nx*ny*nz);
	
	if (world.rank() == 0) printf("Dimensions: %i %i %i\n", nx, ny, nz);
	if (world.rank() == 0) printf("Number of particles: %i\n", nparticles);
	
	// start_time = wall_time();
	auto start_time = std::chrono::high_resolution_clock::now();
	
	part_init_(&nx, &ny, &nz, &nparticles, &x[0], &y[0], &z[0], &vx[0], &vy[0], &vz[0], &mass[0]);
	
	world.gop.fence();
	auto init_time = std::chrono::high_resolution_clock::now();
	if (world.rank() == 0) printf("\nInitialization time: %f s\n\n", 1e-3*(float)std::chrono::duration_cast<std::chrono::milliseconds>(init_time - start_time).count());
	
	nstep = 3;
	
	for(int step(0); step < nstep; ++step){
		
		world.gop.fence();
		auto step_start_time = std::chrono::high_resolution_clock::now();
		
		//
		// project_density_(&nx, &ny, &nz, &nparticles, &x[0], &y[0], &z[0], &mass[0], &density[0], &step);
		//
		
		world.gop.fence();
		auto step_density_time = std::chrono::high_resolution_clock::now();
		if (world.rank() == 0) printf("\tDensity %i: %f s\n", step, 1e-3*(float)std::chrono::duration_cast<std::chrono::milliseconds>(step_density_time - step_start_time).count());

	
		//
		// real_function_3d potential = solve_potential(world, nx, ny, nz, &density[0]);
		real_function_3d potential = solve_potential(world, &x[0], &y[0], &z[0], nx, ny, nz, nparticles, &density[0]);
		//
		
		world.gop.fence();
		auto step_potential_time  = std::chrono::high_resolution_clock::now();
		if (world.rank() == 0) printf("\tPotential %i: %f s\n", step, 1e-3*(float)std::chrono::duration_cast<std::chrono::milliseconds>(step_potential_time  - step_density_time).count());
	
		//
		update_particles(world, &x[0], &y[0], &z[0], &vx[0], &vy[0], &vz[0], nparticles, potential, timestep);
		//
		
		world.gop.fence();
		auto step_update_time = std::chrono::high_resolution_clock::now();
		if (world.rank() == 0) printf("\tUpdate %i: %f s\n", step,  1e-3*(float)std::chrono::duration_cast<std::chrono::milliseconds>(step_update_time  - step_potential_time).count());
		
		//
		memset(&density[0], 0, sizeof(real_t)*nx*ny*nz);
		//
		
		world.gop.fence();
		auto step_finish_time = std::chrono::high_resolution_clock::now();
		if (world.rank() == 0) printf("\nStep %i took %f s\n\n", step, 1e-3*(float)std::chrono::duration_cast<std::chrono::milliseconds>(step_finish_time  - step_start_time).count());
			
	}
	
	world.gop.fence();
	auto overall_time = std::chrono::high_resolution_clock::now();
	if (world.rank() == 0) printf("\nOverall time: %f s\n\n", 1e-3*(float)std::chrono::duration_cast<std::chrono::milliseconds>(overall_time  - start_time).count());

	finalize();
	
	return 0;
}