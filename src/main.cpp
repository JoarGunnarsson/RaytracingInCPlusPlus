#include <iostream>
#include <fstream>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "vec3.h"
#include "colors.h"
#include "denoise.h"
#include "objects.h"
#include "camera.h"
#include "utils.h"
#include "constants.h"
#include "objectunion.h"


void print_pixel_color(const vec3& rgb, std::ofstream& file){
    file << int(255 * rgb[0]) << ' ' << int(255 * rgb[1]) << ' ' << int(255 * rgb[2]) << '\n';
}


struct Scene{
    Object** objects;
    int number_of_objects;
    Camera* camera;
    MaterialManager* material_manager;
    Medium* medium;
};


struct PixelData{
    vec3 pixel_color = vec3(0,0,0);
    vec3 pixel_position = vec3(0,0,0);
    vec3 pixel_normal = vec3(0,0,0);
};


PixelData raytrace(Ray ray, Object** objects, const int number_of_objects, Medium* background_medium){
    MediumStack medium_stack = MediumStack();
    medium_stack.add_medium(background_medium, -1);
    PixelData data;
    vec3 color = vec3(0,0,0);
    vec3 throughput = vec3(1,1,1);
    double random_threshold = 1;
    bool allow_recursion = true;
    bool has_hit_surface = false;

    vec3 saved_point;
    double scatter_pdf;

    for (int depth = 0; depth <= constants::max_recursion_depth; depth++){
        Medium* medium = medium_stack.get_medium();
        double scatter_distance = medium -> sample_distance();

        ray.t_max = scatter_distance;
        Hit ray_hit;
        if (!find_closest_hit(ray_hit, ray, objects, number_of_objects)){
            if (scatter_distance == constants::max_ray_distance){
                break;
            }
            ray_hit.distance = constants::max_ray_distance;
        }
        // save refractive indices here? Take from hit object + current/next medium?
        
        bool scatter = scatter_distance < ray_hit.distance;
        scatter_distance = std::min(scatter_distance, ray_hit.distance);
        if (scatter){
            color += medium -> sample_emission() * throughput;
        }

        throughput *= medium -> sample(objects, number_of_objects, scatter_distance, scatter);
        
        if (scatter){
            vec3 scatter_point = ray.starting_position + ray.direction_vector * scatter_distance;
            vec3 scattered_direction = medium -> sample_direction(ray.direction_vector);
            if (constants::enable_next_event_estimation){
                ray_hit.intersection_point = scatter_point;

                color += sample_light(ray_hit, objects, number_of_objects, medium_stack, true) * throughput;

                ray.type = DIFFUSE;
                scatter_pdf = medium -> phase_function(ray.direction_vector, scattered_direction);
                saved_point = scatter_point;
            }
            
            ray.starting_position = scatter_point;
            ray.direction_vector = scattered_direction;
        }     
        else{
            if (!has_hit_surface){
                data.pixel_position = ray_hit.intersection_point;
                data.pixel_normal = ray_hit.normal_vector;
                has_hit_surface = true;
            }

            bool is_specular_ray = ray.type == REFLECTED || ray.type == TRANSMITTED;
            Object* hit_object = objects[ray_hit.intersected_object_index];

            // If a light source is hit, compute the light_pdf based on the saved_point (previous hitpoint) and use MIS to add the light.
            // Could move this to a separate function, make it clearer what it is doing.
            if (hit_object -> is_light_source()){
                double weight;
                if (!constants::enable_next_event_estimation || depth == 0 || is_specular_ray){
                    weight = 1;
                }
                else{
                    double light_pdf = objects[ray_hit.intersected_object_index] -> light_pdf(ray_hit.intersection_point, saved_point, ray_hit.primitive_ID);
                    weight = mis_weight(1, scatter_pdf, 1, light_pdf);
                }
                vec3 light_emittance = hit_object -> get_light_emittance(ray_hit);

                color += weight * light_emittance * throughput; // TODO: What does this dot product do?  (dot_vectors(ray.direction_vector, ray_hit.normal_vector) < 0
            }
            
            if (constants::enable_next_event_estimation){
                color += sample_light(ray_hit, objects, number_of_objects, medium_stack, false) * throughput;//hit_object -> sample_direct(ray_hit, objects, number_of_objects, medium_stack) * throughput;
            }
            
            BrdfData brdf_result = hit_object -> sample(ray_hit);
            // TODO: Rename allow_direct_light!
            // TODO: Rename is_virtual_surface variable...
            bool is_virtual_surface = hit_object -> get_material(ray_hit.primitive_ID) -> allow_direct_light(); //This deviates from usual pattern of object method calling material method, but is better?
            if (is_virtual_surface){
                brdf_result.type = ray.type;
            }
            else{ 
                scatter_pdf = brdf_result.pdf;
                saved_point = ray_hit.intersection_point;
            }
            throughput *= brdf_result.brdf_over_pdf;

            double incoming_dot_normal = dot_vectors(ray_hit.incident_vector, ray_hit.normal_vector);
            double outgoing_dot_normal = dot_vectors(brdf_result.outgoing_vector, ray_hit.normal_vector);
            
            bool penetrating_boundary = incoming_dot_normal * outgoing_dot_normal > 0;

            // TODO: Do the below part before sampling, so we can get the correct medium for refractive index etc?
            // TODO: Can save current_medium and next_medium, and pass that into sample and compute_direct_light.
            Medium* new_medium = hit_object -> get_material(ray_hit.primitive_ID) -> medium;
            if (penetrating_boundary && new_medium){
                // Something about this is not really working, tries to pop medium while medium is not in stack. We enter multple times too.
                // Seems to be an issue with concave objects, since the issue is not present for convex object unions (sphere etc).
                // Probably due to numeric errors. Currently relatively rare, so can be ignored, but not very good.
                if (ray_hit.outside){
                    medium_stack.add_medium(new_medium, ray_hit.intersected_object_index);
                }
                else{
                    medium_stack.pop_medium(ray_hit.intersected_object_index);
                }
            }
            ray.starting_position = ray_hit.intersection_point;
            ray.direction_vector = brdf_result.outgoing_vector;
            ray.type = brdf_result.type;
        }

        if (depth < constants::force_tracing_limit){
            random_threshold = 1;
            allow_recursion = true;
        }
        else{
            random_threshold = std::min(throughput.max(), 0.9);
            double random_value = random_uniform(0, 1);
            allow_recursion = random_value < random_threshold;
        }
        
        if (!allow_recursion){
            break;
        }

        throughput /= random_threshold;
    }
    
    data.pixel_color = color;
    return data;
 }


PixelData compute_pixel_color(const int x, const int y, const Scene& scene){
    PixelData data;
    vec3 pixel_color = vec3(0,0,0);
    for (int i = 0; i < constants::samples_per_pixel; i++){
        Ray ray;
        ray.starting_position = scene.camera -> position;
        ray.type = TRANSMITTED;
        double new_x = x;
        double new_y = y;

        if (constants::enable_anti_aliasing){
            new_x += random_normal() / 3.0;
            new_y += random_normal() / 3.0;
        }
        
        ray.direction_vector = scene.camera -> get_starting_directions(new_x, new_y);
        PixelData sampled_data = raytrace(ray, scene.objects, scene.number_of_objects, scene.medium);
        data.pixel_position = sampled_data.pixel_position;
        data.pixel_normal = sampled_data.pixel_normal;
        pixel_color += sampled_data.pixel_color;    
    }

    data.pixel_color = pixel_color / (double) constants::samples_per_pixel;
    data.pixel_position = data.pixel_position / (double) constants::samples_per_pixel;
    data.pixel_normal = data.pixel_normal / (double) constants::samples_per_pixel;
    return data;
}


void print_progress(double progress){
    if (progress <= 1.0) {
        int bar_width = 60;

        std::clog << "[";
        int pos = bar_width * progress;
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::clog << "=";
            else if (i == pos) std::clog << ">";
            else std::clog << " ";
        }
        std::clog << "] " << int(progress * 100.0) << " %\r";
    }
}


Scene create_scene(){

    MaterialManager* manager = new MaterialManager();
    MaterialData white_data;
    white_data.albedo_map = new ValueMap3D(colors::WHITE * 0.7);
    DiffuseMaterial* white_diffuse_material = new DiffuseMaterial(white_data);
    manager -> add_material(white_diffuse_material);
    
    MaterialData white_reflective_data;
    white_reflective_data.albedo_map = new ValueMap3D(colors::WHITE * 0.8);
    ReflectiveMaterial* white_reflective_material = new ReflectiveMaterial(white_reflective_data);   
    manager -> add_material(white_reflective_material);

    MaterialData red_material_data;
    red_material_data.albedo_map = new ValueMap3D(colors::RED);
    DiffuseMaterial* red_diffuse_material = new DiffuseMaterial(red_material_data);
    manager -> add_material(red_diffuse_material);

    MaterialData green_material_data;
    green_material_data.albedo_map = new ValueMap3D(colors::GREEN);
    DiffuseMaterial* green_diffuse_material = new DiffuseMaterial(green_material_data);
    manager -> add_material(green_diffuse_material);

    MaterialData gold_data;
    gold_data.albedo_map = new ValueMap3D(colors::GOLD);
    gold_data.roughness_map = new ValueMap1D(0.3);
    gold_data.refractive_index = 0.277;
    gold_data.extinction_coefficient = 2.92;
    gold_data.is_dielectric = false;
    MetallicMicrofacet* gold_material = new MetallicMicrofacet(gold_data);
    manager -> add_material(gold_material);

    MaterialData light_material_data;
    light_material_data.albedo_map =  new ValueMap3D(colors::WHITE * 0.8);
    light_material_data.emission_color_map =  new ValueMap3D(colors::WARM_WHITE);
    light_material_data.light_intensity_map = new ValueMap1D(200.0);
    light_material_data.is_light_source = true;
    DiffuseMaterial* light_source_material = new DiffuseMaterial(light_material_data);
    manager -> add_material(light_source_material);

    MaterialData glass_data;
    glass_data.refractive_index = 1.5;
    BeersLawMedium* glass_medium = new BeersLawMedium(vec3(0), (vec3(1,1,1) - colors::BLUE) * 0.0, vec3(0));
    glass_data.medium = glass_medium;
    TransparentMaterial* glass_material = new TransparentMaterial(glass_data);
    manager -> add_material(glass_material);

    MaterialData scattering_glass_data;
    scattering_glass_data.refractive_index = 1.33;
    ScatteringMediumHomogenous* scattering_glass_medium = new ScatteringMediumHomogenous(vec3(0.2, 0.2, 0.3)*0, vec3(2.7,1.,1.1)*0.5, vec3(0));
    scattering_glass_data.medium = scattering_glass_medium;
    TransparentMaterial* scattering_glass_material = new TransparentMaterial(scattering_glass_data);
    manager -> add_material(scattering_glass_material);

    MaterialData mirror_data;
    ReflectiveMaterial* mirror_material = new ReflectiveMaterial(mirror_data);
    manager -> add_material(mirror_material);

    Plane* this_floor = new Plane(vec3(0,0,0), vec3(1,0,0), vec3(0,0,-1), white_diffuse_material);
    Rectangle* front_wall = new Rectangle(vec3(0,1.55,-0.35), vec3(1,0,0), vec3(0,1,0), 2, 1.55*2, white_diffuse_material);
    Rectangle* left_wall = new Rectangle(vec3(-1,1.55,1.575), vec3(0,0,-1), vec3(0,1,0), 3.85, 1.55*2, white_diffuse_material);
    Rectangle* right_wall = new Rectangle(vec3(1,1.55,1.575), vec3(0,0,1), vec3(0,1,0), 3.85, 1.55*2, white_diffuse_material);
    Plane* roof = new Plane(vec3(0,2.2,0), vec3(1,0,0), vec3(0,0,1), white_diffuse_material);
    Rectangle* back_wall = new Rectangle(vec3(0,1.55,3.5), vec3(0,1,0), vec3(1,0,0), 3.85, 1.55*2, white_diffuse_material);

    Sphere* ball1 = new Sphere(vec3(0, 0.8, 1), 0.35, glass_material);
    Sphere* ball2 = new Sphere(vec3(-0.3, 0.3, 1.3), 0.2, gold_material);

    Sphere* light_source = new Sphere(vec3(-1, 2.199, 1), 0.2, light_source_material);
    
    double desired_size = 0.6;
    vec3 desired_center = vec3(-0.3, 0.1, 1.3);
    bool smooth_shade = false;
    bool transform_object = true;
    // TODO: Actually, use struct called object_transform, can set it to nullptr if no transformation should be made.
    ObjectUnion* loaded_model = load_object_model("./models/water_cube.obj", scattering_glass_material, smooth_shade, transform_object, desired_center, desired_size);

    int number_of_objects = 8;
    Object** objects = new Object*[number_of_objects]{this_floor, front_wall, left_wall, right_wall, roof, back_wall, light_source, loaded_model};

    ScatteringMediumHomogenous* background_medium = new ScatteringMediumHomogenous(vec3(0.), (colors::WHITE) * 0.0, vec3(0));

    vec3 camera_position = vec3(-1, 0.5, 2.2);
    vec3 viewing_direction = vec3(0.8, -0.3, -1);
    vec3 screen_y_vector = vec3(0, 1, 0);
    Camera* camera = new Camera(camera_position, viewing_direction, screen_y_vector);

    Scene scene;
    scene.objects = objects;
    scene.camera = camera;
    scene.number_of_objects = number_of_objects;
    scene.material_manager = manager;
    scene.medium = background_medium;
    return scene;
}


void clear_scene(Scene& scene){
    for (int i = 0; i < scene.number_of_objects; i++){
        delete scene.objects[i];
    }

    delete[] scene.objects;
    delete scene.material_manager;
    delete scene.camera;
    delete scene.medium;
}


void raytrace_section(const int start_idx, const int number_of_pixels, const Scene& scene, double* image, vec3* position_buffer, vec3* normal_buffer){
    for (int i = 0; i < number_of_pixels; i++){
        int idx = start_idx + i;

        int x = idx % constants::WIDTH;
        int y = constants::HEIGHT - idx / constants::WIDTH;
        PixelData data = compute_pixel_color(x, y, scene);
        
        vec3 pixel_color = tone_map(data.pixel_color);
        for (int j = 0; j < 3; j++){
            image[3*idx+j] = pixel_color[j];
        }

        position_buffer[idx] = data.pixel_position;
        normal_buffer[idx] = data.pixel_normal;
    }
    std::clog << "Thread complete.\n";
}


void run_denoising(double* pixel_buffer, vec3* position_buffer, vec3* normal_buffer){
    denoise(pixel_buffer, position_buffer, normal_buffer);
}


double* create_mmap(const char* filepath, const size_t file_size, int& fd){
    fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, file_size) == -1) {
        perror("Error setting file size");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    double* mmap_file = static_cast<double*>(mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (mmap_file == MAP_FAILED) {
        perror("Error mapping file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return mmap_file;
}


void close_mmap(double* mmap_file, const size_t file_size, const int fd){
    if (munmap(mmap_file, file_size) == -1) {
        perror("Error un-mapping the file");
    }
    close(fd);
}


int main() {
    std::chrono::steady_clock::time_point begin_build = std::chrono::steady_clock::now();

    Scene scene = create_scene();

    std::chrono::steady_clock::time_point end_build = std::chrono::steady_clock::now();
    std::clog << "Time taken to build scene: " << std::chrono::duration_cast<std::chrono::seconds>(end_build - begin_build).count() << "[s]" << std::endl;
    
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    size_t FILESIZE = constants::WIDTH * constants::HEIGHT * 3 * sizeof(double);
    vec3* position_buffer = new vec3[constants::WIDTH * constants::HEIGHT];
    vec3* normal_buffer = new vec3[constants::WIDTH * constants::HEIGHT];

    int number_of_threads = std::thread::hardware_concurrency();
    std::clog << "Running program with number of threads: " << number_of_threads << ".\n";
    std::thread thread_array[number_of_threads];

    int image_fd;
    double *image = create_mmap(constants::raw_file_name, FILESIZE, image_fd);

    int pixels_per_thread = std::ceil(constants::WIDTH * constants::HEIGHT / (double) number_of_threads);
    for (int i = 0; i < number_of_threads; i++){
        int start_idx = pixels_per_thread * i;
        int pixels_to_handle = std::min(pixels_per_thread, constants::WIDTH * constants::HEIGHT - i * pixels_per_thread);
        thread_array[i] = std::thread(raytrace_section, start_idx, pixels_to_handle, scene, image, position_buffer, normal_buffer);
    }

    for (int i = 0; i < number_of_threads; i++){
        thread_array[i].join();
    }

    std::cout << constants::WIDTH << std::endl;

    print_progress(1);
    std::clog << std::endl;

    if (constants::enable_denoising){
        int denoised_image_fd;
        double *denoised_image = create_mmap(constants::raw_denoised_file_name, FILESIZE, denoised_image_fd);
 
        for (int i = 0; i < constants::WIDTH * constants::HEIGHT * 3; i++){
            denoised_image[i] = image[i];
        }
        run_denoising(denoised_image, position_buffer, normal_buffer);
        close_mmap(denoised_image, FILESIZE, denoised_image_fd);
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::clog << "Time taken: " << std::chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "[s]" << std::endl;

    close_mmap(image, FILESIZE, image_fd);

    clear_scene(scene);

    delete[] position_buffer;
    delete[] normal_buffer;
    return 0;
}