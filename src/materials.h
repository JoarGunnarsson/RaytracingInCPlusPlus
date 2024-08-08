#ifndef materials
#define materials
#include "vec3.h"
#include "colors.h"
#include "utils.h"
#include "constants.h"
#include "objects.h"

class Object;

Hit findClosestHit(const Ray& ray, Object** objects, const int size);

struct brdfData{
    vec3 outgoingVector;
    vec3 brdfMultiplier;
    bool specular = false;
};

class Material{
    public:
        vec3 albedo;
        double refractiveIndex;
        double attenuationCoefficient;
        vec3 absorptionAlbedo;
        vec3 emmissionColor;
        double lightIntensity;
        bool isDielectric;
        double imaginaryRefractiveIndex;
        Material(){
            albedo = WHITE;
            refractiveIndex = 1;
            attenuationCoefficient = 0;
            absorptionAlbedo = WHITE;
            attenuationCoefficient = 0;
            emmissionColor = WHITE;
            lightIntensity = 0;
            imaginaryRefractiveIndex = 1.5;
        }
        Material(vec3 _diffuseColor, double _diffuseCoefficient=0.8, double _refractiveIndex=1, double _attenuationCoefficient=0,
        vec3 _emmissionColor=WHITE, double _lightIntensity=0, bool _isDielectric=true, double _imaginaryRefractiveIndex=0){
            albedo = _diffuseColor * _diffuseCoefficient;
            refractiveIndex = _refractiveIndex;
            attenuationCoefficient = _attenuationCoefficient;
            absorptionAlbedo = vec3(1,1,1) - albedo;
            emmissionColor = _emmissionColor;
            lightIntensity = _lightIntensity;
            isDielectric = _isDielectric;
            imaginaryRefractiveIndex = _imaginaryRefractiveIndex;
        }

    virtual vec3 eval(){
        throw VirtualMethodNotAllowedException("this is a pure virtual method and should not be called.");
        vec3 vec;
        return vec;
    }

    virtual brdfData sample(const Hit& hit, Object** objectPtrList, const int numberOfObjects){
        throw VirtualMethodNotAllowedException("this is a pure virtual method and should not be called.");
        brdfData data;
        return data;
    }

    vec3 getLightEmittance(){
        return emmissionColor * lightIntensity;
    }
};


class MicrofacetMaterial : public Material{
    public:
        MicrofacetMaterial(){
            albedo = WHITE;
            refractiveIndex = 1;
            attenuationCoefficient = 0;
            absorptionAlbedo = WHITE;
            attenuationCoefficient = 0;
            emmissionColor = WHITE;
            lightIntensity = 0;
        }
        MicrofacetMaterial(vec3 _diffuseColor, double _diffuseCoefficient=0.8, double _refractiveIndex=1, double _attenuationCoefficient=0,
        vec3 _emmissionColor=WHITE, double _lightIntensity=0){
            albedo = _diffuseColor * _diffuseCoefficient;
            refractiveIndex = _refractiveIndex;
            attenuationCoefficient = _attenuationCoefficient;
            absorptionAlbedo = vec3(1,1,1) - albedo;
            emmissionColor = _emmissionColor;
            lightIntensity = _lightIntensity;
        }

    vec3 eval() override{
        return BLACK;
    }

    brdfData sample(const Hit& hit, Object** objectPtrList, const int numberOfObjects) override{
        brdfData data;
        return data;
    }
};


class DiffuseMaterial : public Material{
    public:
        using Material::Material;

    vec3 eval() override{
        return albedo / M_PI;
    }

    brdfData sample(const Hit& hit, Object** objectPtrList, const int numberOfObjects) override{
        vec3 outgoingVector = sampleCosineHemisphere(hit.normalVector);
        vec3 brdfMultiplier = albedo;
        brdfData data;
        data.outgoingVector = outgoingVector;
        data.brdfMultiplier = brdfMultiplier;
        data.specular = false;
        return data;
    }
};


class ReflectiveMaterial : public Material{
    public:
        using Material::Material;

    vec3 eval() override{
        return BLACK;
    }

    brdfData sample(const Hit& hit, Object** objectPtrList, const int numberOfObjects) override{
        vec3 outgoingVector = reflectVector(hit.incomingVector, hit.normalVector);
        brdfData data;
        data.outgoingVector = outgoingVector;
        data.brdfMultiplier = albedo;
        data.specular = true;
        return data;
    }
};


class TransparentMaterial : public Material{
    public:
        using Material::Material;

    vec3 eval() override{
        return BLACK;
    }

    brdfData sample(const Hit& hit, Object** objectPtrList, const int numberOfObjects) override{
        double incomingDotNormal = dotVectors(hit.incomingVector, hit.normalVector);
        vec3 fresnelNormal;
        bool inside = incomingDotNormal > 0.0;
        double n1;
        double k1;
        double n2;
        double k2;
        if (!inside){
            fresnelNormal = -hit.normalVector;
            n1 = constants::airRefractiveIndex;
            k1 = 0;
            n2 = refractiveIndex;
            k2 = imaginaryRefractiveIndex;
        }
        else{
            fresnelNormal = hit.normalVector;
            n1 = refractiveIndex;
            k1 = imaginaryRefractiveIndex;
            n2 = constants::airRefractiveIndex;
            k2 = 0;
        }

        vec3 transmittedVector = refractVector(fresnelNormal, hit.incomingVector, n1, n2);

        double F_r = 1;
        if (transmittedVector.length_squared() != 0){
            F_r = fresnelMultiplier(hit.incomingVector, -fresnelNormal, n1, k1, n2, k2, isDielectric);
        }

        double randomNum = randomUniform(0, 1);
        bool isReflected = randomNum <= F_r;
        
        vec3 brdfMultiplier;
        vec3 outgoingVector;
        if (isReflected && isDielectric){
            brdfMultiplier = WHITE;
            outgoingVector = reflectVector(hit.incomingVector, -fresnelNormal);
        }
        else if (isReflected && !isDielectric){
            brdfMultiplier = albedo;
            outgoingVector = reflectVector(hit.incomingVector, -fresnelNormal);
        }
        else{
            Ray transmissionRay;
            transmissionRay.directionVector = transmittedVector;
            transmissionRay.startingPosition = hit.intersectionPoint;
            Hit transmissionHit = findClosestHit(transmissionRay, objectPtrList, numberOfObjects);
            vec3 attenuationColor;
            double distance = transmissionHit.distance;
            if (distance > 0 && !inside){
                vec3 log_attenuation = absorptionAlbedo * attenuationCoefficient * (-distance);
                attenuationColor = albedo * expVector(log_attenuation);
            }
            else{
                attenuationColor = albedo;
            }

            double refractionIntensityFactor = pow(n2 / n1, 2);
            brdfMultiplier = attenuationColor * refractionIntensityFactor;
            outgoingVector = transmittedVector;
        }
        brdfData data;
        data.outgoingVector = outgoingVector;
        data.brdfMultiplier = brdfMultiplier;
        data.specular = true;
        return data;
    }
};


#endif