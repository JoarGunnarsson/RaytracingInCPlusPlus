#ifndef MEDIUM_H
#define MEDIUM_H

#include <stdexcept>
#include "utils.h"
#include "objects.h"

class Object;


int sample_random_light_2(Object** objects, const int number_of_objects, int& number_of_light_sources);
vec3 direct_lighting_2(const vec3& point, Object** objects, const int number_of_objects);


class Medium{
    public:
        int id;
        double scattering_coefficient;
        vec3 absorption_albedo;
        vec3 extinction_albedo;

        Medium(const double _scattering_coefficient, const vec3& _absorption_albedo);

        virtual double sample_distance() const;
        virtual vec3 sample_direction(const vec3& incident_vector) const;
        virtual void Integrate(Object** objects, const int number_of_objects, const Ray& incoming_ray, vec3& Lv, vec3& transmittance, vec3& weight, Ray& outgoing_ray);
};


class BeersLawMedium: public Medium{
    public:
        using Medium::Medium;

    vec3 transmittance_color(const double distance) const;
    virtual void Integrate(Object** objects, const int number_of_objects, const Ray& incoming_ray, vec3& Lv, vec3& transmittance, vec3& weight, Ray& outgoing_ray) override;
        
};



class SingleScatteringHomogenousMedium: public Medium{
    public:
        using Medium::Medium;

    vec3 transmittance_color(const double distance) const;
    virtual void Integrate(Object** objects, const int number_of_objects, const Ray& incoming_ray, vec3& L, vec3& transmittance, vec3& weight, Ray& outgoing_ray) override;

};


class MediumStack{
    public:
        const int MAX_STACK_SIZE = 50;
        int stack_size = 0;
        Medium** medium_stack = new Medium*[MAX_STACK_SIZE];

        MediumStack(){}
        ~MediumStack();

        Medium* get_medium() const;
        void add_medium(Medium* medium, const int id);
        void pop_medium(const int id);
};

#endif