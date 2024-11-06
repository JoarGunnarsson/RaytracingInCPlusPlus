#ifndef OBJECTUNION_H
#define OBJECTUNION_H

#include <fstream>
#include "constants.h"
#include "vec3.h"
#include "utils.h"
#include "objects.h"


vec3 get_max_point(Object** triangles, int number_of_triangles){
    if (number_of_triangles == 0){
        return vec3(0,0,0);
    }
    vec3 max_point = triangles[0] -> max_axis_point();
    for (int i = 1; i < number_of_triangles; i++){
        vec3 this_max_point = triangles[i] -> max_axis_point();
        for (int j = 0; j < 3; j++){
            max_point.e[j] = std::max(this_max_point[j], max_point[j]);
        }
    }
    return max_point;
}


vec3 get_min_point(Object** triangles, int number_of_triangles){
    if (number_of_triangles == 0){
        return vec3(0,0,0);
    }
    vec3 min_point = triangles[0] -> min_axis_point();
    for (int i = 1; i < number_of_triangles; i++){
        vec3 this_min_point = triangles[i] -> min_axis_point();
        for (int j = 0; j < 3; j++){
            min_point.e[j] = std::min(this_min_point[j], min_point[j]);
        }
    }
    return min_point;
}


class BoundingBox{
    public:
        vec3 p1;
        vec3 p2;
        double width;
        double height;
        double length;
        double axis_length[3];
        BoundingBox(){}
        BoundingBox(Object** _triangles, int number_of_triangles){
            p1 = get_min_point(_triangles, number_of_triangles);
            p2 = get_max_point(_triangles, number_of_triangles);

            width = p2[0] - p1[0];
            length = p2[1] - p1[1];
            height = p2[2] - p1[2];
            axis_length[0] = width;
            axis_length[1] = length;
            axis_length[2] = height;
        }
    
    inline bool is_within_bounds(const double x, const double lower, const double higher){
        return lower <= x && x <= higher;
    }

    double intersect(const Ray& ray){
        double t[6];
        bool inside_bounds[6];
        for (int i = 0; i < 3; i++){
            if (std::abs(ray.direction_vector[i]) < constants::EPSILON){
                t[i] = -1;
                inside_bounds[i] = false;
                continue;
            }
            t[i] = (p1[i] - ray.starting_position[i]) / ray.direction_vector[i];
            vec3 hit_point = ray.direction_vector * t[i] + ray.starting_position;
            vec3 difference_vector = hit_point - p1;
            if (i == 0){
                inside_bounds[i] = is_within_bounds(difference_vector[1], 0, length) && is_within_bounds(difference_vector[2], 0, height);
            }
            else if (i ==  1){
                inside_bounds[i] = is_within_bounds(difference_vector[0], 0, width) && is_within_bounds(difference_vector[2], 0, height);
            }
            else if (i == 2){
                inside_bounds[i] = is_within_bounds(difference_vector[0], 0, width) && is_within_bounds(difference_vector[1], 0, length);
            }
        }

        for (int i = 0; i < 3; i++){
            if (std::abs(ray.direction_vector[i]) < constants::EPSILON){
                t[i+3] = -1;
                inside_bounds[i+3] = false;
                continue;
            }
            t[i+3] = (p2[i] - ray.starting_position[i]) / ray.direction_vector[i];
            vec3 hit_point = ray.direction_vector * t[i+3] + ray.starting_position;
            vec3 difference_vector = hit_point - p2;
            if (i == 0){
                inside_bounds[i+3] = is_within_bounds(difference_vector[1], -length, 0) && is_within_bounds(difference_vector[2], -height, 0);
            }
            else if (i ==  1){
                inside_bounds[i+3] = is_within_bounds(difference_vector[0], -width, 0) && is_within_bounds(difference_vector[2], -height, 0);
            }
            else if (i == 2){
                inside_bounds[i+3] = is_within_bounds(difference_vector[0], -width, 0) && is_within_bounds(difference_vector[1], -length, 0);
            }
        }

        double min_t = -1;
        for (int i = 0; i < 6; i++){
            if (inside_bounds[i] && (min_t == -1 || min_t > t[i]) && t[i] > constants::EPSILON){
                min_t = t[i];
            }
        }

        return min_t;
    }    
};


void sort_by_axis(Object** triangles, int number_of_triangles, int axis){
    std::sort(triangles, triangles + number_of_triangles, [axis](Object* obj1, Object* obj2){ 
        return (obj1 -> compute_centroid())[axis] < (obj2 -> compute_centroid())[axis]; 
        });
}


class Node{
    public:
        int leaf_size;
        bool is_leaf_node;
        Node* node1;
        Node* node2;
        int depth;
        Object** triangles;
        int number_of_triangles;
        BoundingBox bounding_box;
        Node(){}
        Node(Object** _triangles, int _number_of_triangles, int _leaf_size=12, int depth=0){
            leaf_size = _leaf_size;
            bounding_box = BoundingBox(_triangles, _number_of_triangles);
            if (_number_of_triangles <= leaf_size){
                triangles = _triangles;
                number_of_triangles = _number_of_triangles;
                is_leaf_node = true;
                return;
            }
            is_leaf_node = false;
            int axis = get_split_axis();

            sort_by_axis(_triangles, _number_of_triangles, axis);
            int split_index = _number_of_triangles / 2;

            Object** node1_triangles = new Object*[split_index];
            Object** node2_triangles = new Object*[_number_of_triangles - split_index];

            for (int i = 0; i < split_index; i++){
                node1_triangles[i] = _triangles[i];
            }
            for (int i = split_index; i < _number_of_triangles; i++){
                node2_triangles[i - split_index] = _triangles[i];
            }

            node1 = new Node(node1_triangles, split_index, _leaf_size, depth+1);
            node2 = new Node(node2_triangles, _number_of_triangles - split_index, _leaf_size, depth+1);
        }

    int get_split_axis(){
        int axis;
        double max_length = 0;
        for (int i = 0; i < 3; i++){
            if (bounding_box.axis_length[i] >= max_length){
                axis = i;
                max_length = bounding_box.axis_length[i];
            }
        }
        return axis;
    }

    void intersect(const Ray& ray, Hit& hit){
        if (is_leaf_node){
            if (number_of_triangles == 0){
                return;
            }
            Hit closest_hit = find_closest_hit(ray, triangles, number_of_triangles);
            if (closest_hit.distance > constants::EPSILON && (closest_hit.distance < hit.distance || hit.distance == -1)){
                hit.distance = closest_hit.distance;
                hit.object_ID = closest_hit.object_ID;
            }
            return;
        }

        double d1 = node1 -> bounding_box.intersect(ray);
        double d2 = node2 -> bounding_box.intersect(ray);
        
        bool node1_hit = d1 > constants::EPSILON && (d1 < hit.distance || hit.distance == -1);
        bool node2_hit = d2 > constants::EPSILON && (d2 < hit.distance || hit.distance == -1);
        
        if (node1_hit && node2_hit){
            if (d1 > d2){
                node1 -> intersect(ray, hit);
                if (d2 < hit.distance || hit.distance == -1){
                    node2 -> intersect(ray, hit);
                }
            }
            else{
                node2 -> intersect(ray, hit);
                if (d1 < hit.distance || hit.distance == -1){
                    node1 -> intersect(ray, hit);
                }
            }

        }
        else if (node1_hit){
            node1 -> intersect(ray, hit);
        }
        else if (node2_hit){
            node2 -> intersect(ray, hit);
        }
    }
};


class BoundingVolumeHierarchy{
    public:
        Node* root_node;
        BoundingVolumeHierarchy(){}
        BoundingVolumeHierarchy(Object** triangles, int number_of_triangles, int leaf_size){
            root_node = new Node(triangles, number_of_triangles, leaf_size);
        }

        Hit intersect(const Ray& ray){
            double distance_to_bounding_box = root_node -> bounding_box.intersect(ray);

            Hit hit;
            hit.distance = -1;
            hit.object_ID = -1;
            if (distance_to_bounding_box > constants::EPSILON){
                root_node -> intersect(ray, hit);
            }
            return hit;
        }
};


class ObjectUnion : public Object{
    public:
        Object** objects;
        int number_of_objects;
        double* cumulative_area;
        int* light_source_conversion_indices;
        int number_of_light_sources;
        BoundingVolumeHierarchy bvh;
        bool use_BVH;
        bool contains_light_source = false;
        ObjectUnion(Object** _objects, const int _number_of_objects, const bool construct_BVH=false) : Object(){
            objects = _objects;
            number_of_objects = _number_of_objects;

            area = 0;
            for (int i = 0; i < number_of_objects; i++){
                area += objects[i] -> area;
                if (objects[i] -> is_light_source()){
                    number_of_light_sources++;
                }
            }

            cumulative_area = new double[number_of_objects];
            light_source_conversion_indices = new int[number_of_light_sources];
            int j = 0;
            for (int i = 0; i < number_of_objects; i++){
                if (!objects[i] -> is_light_source()){
                    continue;
                }
                cumulative_area[j] = objects[i] -> area;
                if (j != 0){
                    cumulative_area[j] += cumulative_area[j-1];
                }
                light_source_conversion_indices[j] = i;
                j++;
            }

            use_BVH = construct_BVH;
            if (construct_BVH){
                bvh = BoundingVolumeHierarchy(_objects, _number_of_objects, 12);
            }

            for (int i = 0; i < number_of_objects; i++){
                objects[i] -> object_ID = i;
                if (objects[i] -> is_light_source()){
                  contains_light_source = true;
                }
            }
        }

        ~ObjectUnion(){
            for (int i = 0; i < number_of_objects; i++){
                delete objects[i];
            }
            delete[] objects;
            delete[] cumulative_area;
            delete[] light_source_conversion_indices;
        }

        bool is_light_source() override{
            return contains_light_source;
        }

        vec3 eval(const Hit& hit) override{
            return objects[hit.object_ID]  -> eval(hit);
        }

        BrdfData sample(const Hit& hit, Object** scene_objects, const int number_of_objects) override{
            return objects[hit.object_ID]  -> sample(hit, scene_objects, number_of_objects);
        }

        vec3 get_light_emittance(const Hit& hit) override{
            return objects[hit.object_ID] -> get_light_emittance(hit);
        }

        Hit find_closest_object_hit(const Ray& ray) override{
            if (use_BVH){
                return bvh.intersect(ray);
            }

            Hit hit = find_closest_hit(ray, objects, number_of_objects);
            hit.object_ID = hit.intersected_object_index;
            hit.intersected_object_index = -1;
            return hit;
        }
        
        vec3 get_normal_vector(const vec3& surfacePoint, const int object_ID) override{
            return objects[object_ID] -> get_normal_vector(surfacePoint, object_ID);
        }

        int sample_random_object_index(){
            double random_area_split = random_uniform(0, area);
            int max = number_of_light_sources - 1;
            int min = 0;
            int index;

            if (cumulative_area[0] >= random_area_split){
                return light_source_conversion_indices[0];
            }

            while (min <= max){
                index = (max - min) / 2 + min;

                if (cumulative_area[index] < random_area_split){
                    min = index + 1;
                }
                else if (cumulative_area[index] == random_area_split || (cumulative_area[index] >= random_area_split && cumulative_area[index-1] < random_area_split)){
                    break;
                }
                else{
                    max = index - 1;
                }
            }

            return light_source_conversion_indices[index];
        }

        vec3 generate_random_surface_point() override{
            return objects[sample_random_object_index()] -> generate_random_surface_point();
        }

        vec3 random_light_point(const vec3& intersection_point, double& inversePDF) override{
            int random_index = sample_random_object_index();
            vec3 random_point = objects[random_index] -> generate_random_surface_point();
            inversePDF = cumulative_area[number_of_light_sources-1] * area_to_angle_PDF_factor(random_point, intersection_point, random_index);
            return random_point;
        }
};


struct DataSizes{
    int num_vertices = 0;
    int num_vertex_UVs = 0;
    int num_vertex_normals = 0;
    int num_triangles = 0;
};


int number_of_char_occurances(const std::string& line, const char character){
    int count = 0;
    for (int i = 0; i < line.length(); i++){
        if (line[i] == character){
            count++;
        }
    }
    return count;
}


std::string get_nth_word(const std::string& line, const char delimiter, const int n){
    int number_of_words = number_of_char_occurances(line, delimiter);

    if (n > number_of_words){
        return "";
    }

    int start = 0;
    int end = 0;
    for (int i = 0; i < n+1; i++){
        start = end;
        end = line.find(delimiter, start)+1;
    }
    return line.substr(start, end - start - 1);
}


DataSizes get_vertex_data_sizes(const std::string& file_name){
    std::ifstream model_file(file_name);
    std::string line;
    DataSizes nums;
    
    while(std::getline(model_file, line)){
        std::string first_word = get_nth_word(line, ' ', 0);

        bool is_vertex = first_word == "v";
        bool is_vertex_UV = first_word == "vt";
        bool is_vertex_normal = first_word == "vn";
        bool is_shape = first_word == "f";
        
        if (is_vertex){
            nums.num_vertices++;
        }
        else if (is_vertex_UV){
            nums.num_vertex_UVs++;
        }
        else if (is_vertex_normal){
            nums.num_vertex_normals++;
        }
        else if (is_shape){
            int number_of_spaces = 0;
            for (int i = 0; i < line.size(); i++){
                if (line.substr(i, 1) == " "){
                    number_of_spaces++;
                }
            }
            
            bool is_triangle = number_of_spaces == 3;
            bool is_quad = number_of_spaces == 4;
            if (is_triangle){
                nums.num_triangles++;
            }

            else if (is_quad){
                nums.num_triangles += 2;
            }
        }
    }
    return nums;
}


void populate_vertex_arrays(const std::string& file_name, vec3* vertex_array, vec3* vertex_UV_array, vec3* vertex_normal_array){
    int vertex_idx = 0;
    int vertex_UV_idx = 0;
    int vertex_normal_idx = 0;

    std::ifstream model_file(file_name);
    std::string line;
    while(std::getline(model_file, line)){
        std::string first_word = get_nth_word(line, ' ', 0);
        bool is_vertex = first_word == "v";
        bool is_vertex_UV = first_word == "vt";
        bool is_vertex_normal = first_word == "vn";

        if (is_vertex){
            double v1 = std::stod(get_nth_word(line, ' ', 1));
            double v2 = std::stod(get_nth_word(line, ' ', 2));
            double v3 = std::stod(get_nth_word(line, ' ', 3));
            vertex_array[vertex_idx] = vec3(v1, v2, v3);
            vertex_idx++;
        }
        else if (is_vertex_UV){
            double u = std::stod(get_nth_word(line, ' ', 1));
            double v = std::stod(get_nth_word(line, ' ', 2));
            vertex_UV_array[vertex_UV_idx] = vec3(u, v, 0);
            vertex_UV_idx++;
        }
        else if (is_vertex_normal){
            double n1 = std::stod(get_nth_word(line, ' ', 1));
            double n2 = std::stod(get_nth_word(line, ' ', 2));
            double n3 = std::stod(get_nth_word(line, ' ', 3));
            vertex_normal_array[vertex_normal_idx] = vec3(n1, n2, n3);
            vertex_normal_idx++;
        }
    }
}


vec3 compute_average_position(const vec3* vertex_array, const int number_of_vertices){
    vec3 avg = vec3(0,0,0);
    for (int i = 0; i < number_of_vertices; i++){
        avg += vertex_array[i];
    }
    return avg / number_of_vertices;
}


double maximum_distance(const vec3& center, const vec3* vertex_array, const int number_of_vertices){
    double max_distance = 0;
    for (int i = 0; i < number_of_vertices; i++){
        double distance = (vertex_array[i] - center).length();
        if (distance > max_distance){
            max_distance = distance;
        }
    }
    return max_distance;
}

void change_vectors(const vec3& desired_center, const double desired_size, vec3* vertex_array, const int number_of_vertices){
    vec3 average_position = compute_average_position(vertex_array, number_of_vertices);
    double max_distance = maximum_distance(average_position, vertex_array, number_of_vertices);

    for (int i = 0; i < number_of_vertices; i++){
        vertex_array[i] = ((vertex_array[i] - average_position) / max_distance + desired_center) * desired_size;
    }
}


struct PopulateVertexVectorData{
    const std::string vertex_data;
    vec3 v;
    bool v_success = false;
    vec3 uv;
    bool uv_success = false;
    vec3 n;
    bool n_success = false;
    const vec3* vertex_array;
    const vec3* vertex_UV_array;
    const vec3* vertex_normal_array;

    PopulateVertexVectorData(const std::string& data, const vec3* vertex_array, const vec3* vertex_UV_array, const vec3* vertex_normal_array)
        : vertex_data(data), vertex_array(vertex_array), vertex_UV_array(vertex_UV_array), vertex_normal_array(vertex_normal_array) {}
};


void populate_vertex_vectors(PopulateVertexVectorData& args){
    std::string v_idx = get_nth_word(args.vertex_data, '/', 0);
    std::string UV_idx = get_nth_word(args.vertex_data, '/', 1);
    std::string n_idx = get_nth_word(args.vertex_data, '/', 2);
    
    if (v_idx != ""){
        args.v = args.vertex_array[std::stoi(v_idx)-1];
        args.v_success = true;
    }

    if (UV_idx != ""){
        args.uv = args.vertex_UV_array[std::stoi(UV_idx)-1];
        args.uv_success = true;
    }

    if (n_idx != ""){
        args.n = args.vertex_normal_array[std::stoi(n_idx)-1];
        args.n_success = true;
    }
}


struct TriangleConstructionArgs{
    const std::string triangle_data;
    const int idx1;
    const int idx2;
    const int idx3;
    Material* material;
    const vec3* vertex_array;
    const vec3* vertex_UV_array;
    const vec3* vertex_normal_array;
    const bool enable_smooth_shading;

    TriangleConstructionArgs(const std::string& data, const int idx1, const int idx2, const int idx3, Material* material, const vec3* vertex_array,
    const vec3* vertex_UV_array, const vec3* vertex_normal_array, const bool enable_smooth_shading) : triangle_data(data), idx1(idx1), idx2(idx2),
    idx3(idx3), material(material), vertex_array(vertex_array), vertex_UV_array(vertex_UV_array), vertex_normal_array(vertex_normal_array), enable_smooth_shading(enable_smooth_shading) {}
};


struct TriangleCreationResult{
    Triangle* triangle;
    bool success = false;
};


TriangleCreationResult construct_triangle(TriangleConstructionArgs& args){
    std::string v1_data = get_nth_word(args.triangle_data, ' ', args.idx1);
    PopulateVertexVectorData data1 = PopulateVertexVectorData(v1_data, args.vertex_array, args.vertex_UV_array, args.vertex_normal_array);
    populate_vertex_vectors(data1);

    std::string v2_data = get_nth_word(args.triangle_data, ' ', args.idx2);
    PopulateVertexVectorData data2 = PopulateVertexVectorData(v2_data, args.vertex_array, args.vertex_UV_array, args.vertex_normal_array);
    populate_vertex_vectors(data2);

    std::string v3_data = get_nth_word(args.triangle_data, ' ', args.idx3);
    PopulateVertexVectorData data3 = PopulateVertexVectorData(v3_data, args.vertex_array, args.vertex_UV_array, args.vertex_normal_array);
    populate_vertex_vectors(data3);

    bool loaded_vertices_successfully = data1.v_success && data2.v_success && data3.v_success;
    
    TriangleCreationResult result;

    if (!loaded_vertices_successfully){
        return result;
    }

    Triangle* triangle = new Triangle(data1.v, data2.v, data3.v, args.material);

    bool loaded_UV_successfully = data1.uv_success && data2.uv_success && data3.uv_success;
    if (loaded_UV_successfully){triangle -> set_vertex_UV(data1.uv, data2.uv, data3.uv);}

    bool loaded_normals_successfully  = data1.n_success && data2.n_success && data3.n_success;
    if (loaded_normals_successfully && args.enable_smooth_shading){
        triangle -> set_vertex_normals(data1.n, data2.n, data3.n);
    }
    result.success = true;
    result.triangle = triangle;
    return result;
}


int populate_triangle_array(std::string file_name, vec3* vertex_array, vec3* vertex_UV_array, vec3* vertex_normal_array, Object** triangle_array, Material* material, const bool enable_smooth_shading){
    std::ifstream model_file(file_name);
    std::string line;
    int shape_idx = 0;
    while(std::getline(model_file, line)){
        std::string first_word = get_nth_word(line, ' ', 0);
        
        bool is_shape = first_word == "f";
        if (!is_shape){
            continue;
        }
        int number_of_spaces = number_of_char_occurances(line, ' ');
        bool is_triangle = number_of_spaces == 3;
        bool is_quad = number_of_spaces == 4;
        if (is_triangle){
            TriangleConstructionArgs args = TriangleConstructionArgs(line, 1, 2, 3, material, vertex_array, vertex_UV_array, vertex_normal_array, enable_smooth_shading);
            TriangleCreationResult result = construct_triangle(args);
            if (!result.success){
                continue;
            }
            triangle_array[shape_idx] = result.triangle;
            shape_idx++;
        }

        else if (is_quad){
            TriangleConstructionArgs args1 = TriangleConstructionArgs(line, 1, 2, 3, material, vertex_array, vertex_UV_array, vertex_normal_array, enable_smooth_shading);
            TriangleCreationResult result1 = construct_triangle(args1);
            if (result1.success){
                triangle_array[shape_idx] = result1.triangle;
                shape_idx++;
            }
            
            TriangleConstructionArgs args2 = TriangleConstructionArgs(line, 1, 3, 4, material, vertex_array, vertex_UV_array, vertex_normal_array, enable_smooth_shading);
            TriangleCreationResult result2 = construct_triangle(args2);
            if (result1.success){
                triangle_array[shape_idx] = result2.triangle;
                shape_idx++;
            }
        }
    }
    return shape_idx;
}


ObjectUnion* load_object_model(std::string file_name, Material* material, const bool enable_smooth_shading){
    DataSizes nums = get_vertex_data_sizes(file_name);

    vec3 vertex_array[nums.num_vertices];
    vec3 vertex_UV_array[nums.num_vertex_UVs];
    vec3 vertex_normal_array[nums.num_vertex_normals];
    populate_vertex_arrays(file_name, vertex_array, vertex_UV_array, vertex_normal_array);

    double desired_size = 0.7;
    vec3 desired_center = vec3(0, 0.2, 1);
    change_vectors(desired_center, desired_size, vertex_array, nums.num_vertices);

    Object** triangles = new Object*[nums.num_triangles];
    int num_valid_triangles = populate_triangle_array(file_name, vertex_array, vertex_UV_array, vertex_normal_array, triangles, material, enable_smooth_shading);
    ObjectUnion* loaded_object = new ObjectUnion(triangles, num_valid_triangles, true);
    return loaded_object;
}

#endif