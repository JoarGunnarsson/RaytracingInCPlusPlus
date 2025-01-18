#include "objects.h"


// ****** Object base class implementation ****** 
Object::Object(Material* _material) : material(_material), area(0.0), primitive_ID(0) {}

vec3 Object::max_axis_point() const { return vec3(); }
vec3 Object::min_axis_point() const { return vec3(); }
vec3 Object::compute_centroid() const { return vec3(); }
vec3 Object::get_UV(const vec3& point) const { return vec3(); }
Material* Object::get_material(const int primitive_ID) const { return material; }
bool Object::is_light_source() const { return material -> is_light_source; }

vec3 Object::eval(const Hit& hit) const{
    vec3 UV = get_UV(hit.intersection_point);
    return material -> eval(hit, UV[0], UV[1]);
}

vec3 Object::sample_direct(const Hit& hit, Object** objects, const int number_of_objects) const{
    bool inside_object = dot_vectors(hit.incident_vector, hit.normal_vector) > 0.0;
    if (inside_object){
        return colors::BLACK;
    }
    vec3 brdf = eval(hit);
    vec3 light_vector;
    vec3 direct = direct_lighting(hit.intersection_point, objects, number_of_objects, light_vector);
    double cosine = dot_vectors(hit.normal_vector, light_vector);
    cosine = std::max(0.0, cosine);
    return brdf * cosine * direct;
}

BrdfData Object::sample(const Hit& hit) const{
    vec3 UV = get_UV(hit.intersection_point);
    return material -> sample(hit, UV[0], UV[1]);
}

vec3 Object::get_light_emittance(const Hit& hit) const{
    vec3 UV = get_UV(hit.intersection_point);
    return material -> get_light_emittance(UV[0], UV[1]);
}

bool Object::find_closest_object_hit(Hit& hit, Ray& ray) const{ return false; }
vec3 Object::get_normal_vector(const vec3& surface_point, const int primitive_ID) const{ return vec3(); }
vec3 Object::generate_random_surface_point() const{ return vec3(); }

double Object::area_to_angle_PDF_factor(const vec3& surface_point, const vec3& intersection_point, const int primitive_ID) const{
    vec3 normal_vector = get_normal_vector(surface_point, primitive_ID);
    vec3 difference_vector = intersection_point - surface_point;
    vec3 vector_to_point = normalize_vector(difference_vector);
    double inverse_PDF = dot_vectors(normal_vector, vector_to_point) / difference_vector.length_squared();
    return std::max(0.0, inverse_PDF);
}

vec3 Object::random_light_point(const vec3& intersection_point, double& inverse_PDF) const{
    vec3 random_point = generate_random_surface_point();
    inverse_PDF = area * area_to_angle_PDF_factor(random_point, intersection_point, 0);
    return random_point;
}

// ****** Sphere class implementation ****** 

/*
Sphere::Sphere(const vec3& _position, const double _radius) : Object(){
    position = _position;
    radius = _radius;
    area = 4 * M_PI * radius * radius;
}
*/

Sphere::Sphere(const vec3& _position, const double _radius, Material*_material) : Object(_material){
    position = _position;
    radius = _radius;
    area = 4 * M_PI * radius * radius;
}

vec3 Sphere::get_UV(const vec3& point) const{
    vec3 unit_sphere_point = (point - position) / radius;
    double x = -unit_sphere_point[0];
    double y = -unit_sphere_point[1];
    double z = -unit_sphere_point[2];
    double u = 0.5 + atan2(z, x) / (2 * M_PI);
    double v = 0.5 + asin(y) / (M_PI);
    return vec3(u, v, 0);
}

bool Sphere::find_closest_object_hit(Hit& hit, Ray& ray) const {
    double dot_product = dot_vectors(ray.direction_vector, ray.starting_position);
    double b = 2 * (dot_product - dot_vectors(ray.direction_vector, position));
    vec3 difference_in_positions = position - ray.starting_position;
    double c = difference_in_positions.length_squared() - radius * radius;
    double distance;
    bool success = solve_quadratic(b, c, distance);

    hit.primitive_ID = primitive_ID;
    hit.distance = distance;
    return success;
}

vec3 Sphere::get_normal_vector(const vec3& surface_point, const int primitive_ID) const {
    vec3 difference_vector = surface_point - position;
    return normalize_vector(difference_vector);
}

vec3 Sphere::generate_random_surface_point() const {
    return sample_spherical() * radius + position;
}

vec3 Sphere::random_light_point(const vec3& intersection_point, double& inverse_PDF) const {
    double distance = (intersection_point - position).length();
    if (distance <= radius){
        vec3 random_point = generate_random_surface_point();
        inverse_PDF = area * area_to_angle_PDF_factor(random_point, intersection_point, 0);
        return random_point;
    }

    double cos_theta_max = sqrt(1 - pow(radius / distance, 2));
    inverse_PDF = 2 * M_PI * (1 - (cos_theta_max));

    double rand = random_uniform(0, 1);
    double cos_theta = 1 + rand * (cos_theta_max-1);
    double sin_theta = sqrt(1 - cos_theta * cos_theta);
    double cos_alpha = (radius * radius + distance * distance - pow(distance * cos_theta - sqrt(radius * radius - pow(distance*sin_theta, 2)), 2)) / (2.0 * distance * radius);
    double sin_alpha = sqrt(1.0 - cos_alpha * cos_alpha);
    
    vec3 x_hat;
    vec3 y_hat;
    vec3 z_hat = get_normal_vector(intersection_point, 0);
    set_perpendicular_vectors(z_hat, x_hat, y_hat);
    double phi = random_uniform(0, 2.0 * M_PI);
    vec3 random_point = x_hat * sin_alpha * cos(phi) + y_hat * sin_alpha * sin(phi) + z_hat * cos_alpha;
    return random_point * radius + position;
}


// ****** Plane class implementation ****** 
Plane::Plane(const vec3& _position, const vec3& _v1, const vec3& _v2, Material*_material) : Object(_material){
    position = _position;
    v1 = normalize_vector(_v1);
    v2 = normalize_vector(_v2);
    vec3 _normal_vector = cross_vectors(v1, v2);
    normal_vector = normalize_vector(_normal_vector);
}

vec3 Plane::get_UV(const vec3& point) const {
    vec3 shifted_point = point - position;
    double u = 1 - dot_vectors(shifted_point, v1) - 0.5;
    double v = 1 - dot_vectors(shifted_point, v2) - 0.5;
    return vec3(u, v, 0);
}

bool Plane::compute_distance_in_centered_system(const vec3& starting_point, const vec3& direction_vector, double& distance) const {
    double direction_dot_normal = -dot_vectors(direction_vector, normal_vector);
    if (std::abs(direction_dot_normal) < constants::EPSILON){
        return false;
    }

    double distances_to_start = dot_vectors(starting_point, normal_vector);
    distance = distances_to_start / direction_dot_normal;
    if (distance < constants::EPSILON){
        return false;
    }
    return true;
}

bool Plane::find_closest_object_hit(Hit& hit, Ray& ray) const {
    vec3 shifted_point = ray.starting_position - position;
    double distance;
    bool success = compute_distance_in_centered_system(shifted_point, ray.direction_vector, distance);
    hit.primitive_ID = primitive_ID;
    hit.distance = distance;
    return success;
}

vec3 Plane::get_normal_vector(const vec3& surface_point, const int primitive_ID) const {
    return normal_vector;
}


// ****** Rectangle class implementation ****** 
Rectangle::Rectangle(const vec3& _position, const vec3& _v1, const vec3& _v2, const double _L1, const double _L2, Material*_material) : Plane(_position, _v1, _v2, _material){
    L1 = _L1;
    L2 = _L2;
    area = L1 * L2;
}
vec3 Rectangle::get_UV(const vec3& point) const {
    vec3 shifted_point = point - position;
    double u = 1 - dot_vectors(shifted_point, v1) / L1 - 0.5;
    double v = 1 - dot_vectors(shifted_point, v2) / L2 - 0.5;
    return vec3(u, v, 0);
}

bool Rectangle::find_closest_object_hit(Hit& hit, Ray& ray) const {
    hit.primitive_ID = primitive_ID;
    hit.distance = -1;

    vec3 shifted_point = ray.starting_position - position;
    double distance;
    bool success = Plane::compute_distance_in_centered_system(shifted_point, ray.direction_vector, distance);
    if (!success){
        return false;
    }
    double direction_dot_v1 = dot_vectors(ray.direction_vector, v1);
    double direction_dot_v2 = dot_vectors(ray.direction_vector, v2);
    double start_dot_v1 = dot_vectors(shifted_point, v1);
    double start_dot_v2 = dot_vectors(shifted_point, v2);

    if (std::abs(start_dot_v1 + direction_dot_v1 * distance) > L1 / 2.0 + constants::EPSILON || std::abs(start_dot_v2 + direction_dot_v2 * distance) > L2 / 2.0 + constants::EPSILON){
        return false;
    }
    hit.distance = distance < ray.t_max ? distance : -1;
    return true;
}

vec3 Rectangle::generate_random_surface_point() const {
    double r1 = random_uniform(-L1/2, L1/2);
    double r2 = random_uniform(-L2/2, L2/2);
    return v1 * r1 + v2 * r2 + position;
}


// ****** Triangle class implementation ****** 
Triangle::Triangle(const vec3& _p1, const vec3& _p2, const vec3& _p3, Material*_material) : Object(_material){
    p1 = _p1;
    p2 = _p2;
    p3 = _p3;

    position = p1;

    v1 = p2 - p1;
    v2 = p3 - p1;
    normal_vector = normalize_vector(cross_vectors(v1, v2));
    v1 = normalize_vector(v1);
    v2 = normalize_vector(cross_vectors(normal_vector, v1));

    x1 = dot_vectors(p1, v1);
    y1 = dot_vectors(p1, v2);
    x2 = dot_vectors(p2, v1);
    y2 = dot_vectors(p2, v2);
    x3 = dot_vectors(p3, v1);
    y3 = dot_vectors(p3, v2);
    det_T = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);

    area = 0.5 * std::abs(x1 * (y2 - y3) + x2 * (y3 - y1) + x3 * (y1 - y2));

    uv1 = vec3(0, 0, 0);
    uv2 = vec3(0, 0, 0);
    uv3 = vec3(0, 0, 0);

    n1 = normal_vector;
    n2 = normal_vector;
    n3 = normal_vector;
}

vec3 Triangle::max_axis_point() const {
    vec3 point;
    for (int i = 0; i < 3; i++){
        point.e[i] = std::max(std::max(p1[i], p2[i]), p3[i]);
    }
    return point;
}

vec3 Triangle::min_axis_point() const {
    vec3 point;
    for (int i = 0; i < 3; i++){
        point.e[i] = std::min(std::min(p1[i], p2[i]), p3[i]);
    }
    return point;
}

vec3 Triangle::compute_centroid() const{
    return (p1 + p2 + p3) / 3.0;
}

void Triangle::set_vertex_UV(const vec3& _uv1, const vec3& _uv2, const vec3& _uv3){
    uv1 = _uv1;
    uv2 = _uv2;
    uv3 = _uv3;
}

void Triangle::set_vertex_normals(const vec3& _n1, const vec3& _n2, const vec3& _n3){
    n1 = _n1;
    n2 = _n2;
    n3 = _n3;
    smooth_shaded = true;
}

vec3 Triangle::get_normal_vector_smoothed(const vec3& surface_point, const int primitive_ID) const{
    vec3 barycentric_vector = compute_barycentric(surface_point);
    return normalize_vector(n1 * barycentric_vector[0] + n2 * barycentric_vector[1] + n3 * barycentric_vector[2]);
}

vec3 Triangle::get_normal_vector(const vec3& surface_point, const int primitive_ID) const {
    if (smooth_shaded){
        return get_normal_vector_smoothed(surface_point, primitive_ID);
    }
    return normal_vector;
}

vec3 Triangle::compute_barycentric(const vec3& point) const{
    double x = dot_vectors(point, v1);
    double y = dot_vectors(point, v2);

    double lambda1 = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / det_T;
    double lambda2 = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / det_T;
    return vec3(lambda1, lambda2, 1.0 - lambda1 - lambda2);
}

vec3 Triangle::get_UV(const vec3& point) const {
    vec3 barycentric_vector = compute_barycentric(point);
    return uv1 * barycentric_vector[0] + uv2 * barycentric_vector[1] + uv3 * barycentric_vector[2];
}

bool Triangle::find_closest_object_hit(Hit& hit, Ray& ray) const {
    vec3 p1t = p1 - ray.starting_position;
    vec3 p2t = p2 - ray.starting_position;
    vec3 p3t = p3 - ray.starting_position;

    p1t = permute(p1t, ray.kx, ray.ky, ray.kz);
    p2t = permute(p2t, ray.kx, ray.ky, ray.kz);
    p3t = permute(p3t, ray.kx, ray.ky, ray.kz);

    p1t[0] += ray.Sx * p1t[2];
    p1t[1] += ray.Sy * p1t[2];
    p2t[0] += ray.Sx * p2t[2];
    p2t[1] += ray.Sy * p2t[2];
    p3t[0] += ray.Sx * p3t[2];
    p3t[1] += ray.Sy * p3t[2];

    double e1 = p2t[0] * p3t[1] - p2t[1] * p3t[0];
    double e2 = p3t[0] * p1t[1] - p3t[1] * p1t[0];
    double e3 = p1t[0] * p2t[1] - p1t[1] * p2t[0];

    if ((e1 < 0 || e2 < 0 || e3 < 0) && (e1 > 0 || e2 > 0 || e3 > 0)){
        return false;
    }

    double det = e1 + e2 + e3;
    if (det == 0){
        return false;
    }

    p1t[2] *= ray.Sz;
    p2t[2] *= ray.Sz;
    p3t[2] *= ray.Sz;

    double t_scaled = e1 * p1t[2] + e2 * p2t[2] + e3 * p3t[2];

    if (det < 0 && (t_scaled < ray.t_max * det) ){
        return false;
    }
    else if (det > 0 && (t_scaled > ray.t_max * det)){
        return false;
    }

    hit.distance = t_scaled / det;
    hit.primitive_ID = primitive_ID;
    return true;
}

vec3 Triangle::generate_random_surface_point() const {
    double r1 = random_uniform(0, 1);
    double r2 = random_uniform(0, 1);
    return p1 * (1.0 - sqrt(r1)) + p2 * (sqrt(r1) * (1.0 - r2)) + p3 * (sqrt(r1) * r2);
}


bool find_closest_hit(Hit& closest_hit, Ray& ray, Object** objects, const int size){
    closest_hit.distance = constants::max_ray_distance;
    bool found_a_hit = false;

    ray.prepare();
    for (int i = 0; i < size; i++){
        Hit hit;
        bool success = objects[i] -> find_closest_object_hit(hit, ray);
        if (success && hit.distance > constants::EPSILON && hit.distance < closest_hit.distance){ // hit.distance > constants::EPSILON , should not be necessary.
            hit.intersected_object_index = i;
            closest_hit = hit;
            ray.t_max = hit.distance;
            found_a_hit = true;
        }
    }
    if (!found_a_hit){
        return false;
    }

    closest_hit.intersection_point = ray.starting_position + ray.direction_vector * closest_hit.distance;
    closest_hit.normal_vector = objects[closest_hit.intersected_object_index] -> get_normal_vector(closest_hit.intersection_point, closest_hit.primitive_ID);
    closest_hit.incident_vector = ray.direction_vector;
    return true;
 }


int sample_random_light(Object** objects, const int number_of_objects, int& number_of_light_sources){
    int light_source_idx_array[number_of_objects];

    number_of_light_sources = 0;
    
    for (int i = 0; i < number_of_objects; i++){
        if (objects[i] -> is_light_source()){
            light_source_idx_array[number_of_light_sources] = i;
            number_of_light_sources++;
        }
    }
    if (number_of_light_sources == 0){
        return -1;
    }
    
    int random_index = random_int(0, number_of_light_sources);
    int light_index = light_source_idx_array[random_index];
    return light_index;
}



vec3 direct_lighting(const vec3& point, Object** objects, const int number_of_objects, vec3& sampled_direction){
    int number_of_light_sources;
    int light_index = sample_random_light(objects, number_of_objects, number_of_light_sources);
    if (light_index == -1){
        return colors::BLACK;
    }

    double inverse_PDF;
    vec3 random_point = objects[light_index] -> random_light_point(point, inverse_PDF);

    sampled_direction = random_point -point;
    double distance_to_light = sampled_direction.length();
    sampled_direction = normalize_vector(sampled_direction);
    
    Ray light_ray;
    light_ray.starting_position = point;
    light_ray.direction_vector = sampled_direction;

    // This does not need to be a full find_closest hit, we can return early. Should allow parameter ignore_index.
    Hit light_hit;
    find_closest_hit(light_hit, light_ray, objects, number_of_objects);
    bool in_shadow = light_hit.intersected_object_index != light_index;
    bool same_distance = std::abs(distance_to_light - light_hit.distance) <= constants::EPSILON;
    //bool hit_from_behind = dot_vectors(vector_towards_light, hit.normal_vector) < 0.0;

    if ( in_shadow || !same_distance){
        return colors::BLACK;
    }

    vec3 light_emittance = objects[light_index] -> get_light_emittance(light_hit);

    return light_emittance * inverse_PDF * (double) number_of_light_sources;
}
