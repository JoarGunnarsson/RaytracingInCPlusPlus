#ifndef OBJECTS_H
#define OBJECTS_H
#include "vec3.h"
#include "utils.h"
#include "materials.h"
#include "constants.h"
#include "colors.h"


class Object{
    public:
        vec3 position;
        Material* material;
        double area;
        Object(){}
        Object(vec3 _position, Material* _material){
            position = _position;
            material = _material;
        }
        virtual ~Object(){}

        virtual Hit findClosestHit(const Ray& ray){
            Hit hit;
            return hit;
        }
        virtual vec3 getNormalVector(const vec3& intersectionPoint){
            vec3 vec;
            return vec;
        }
        virtual vec3 generateRandomSurfacePoint(){
            vec3 point;
            return point;
        }

        virtual vec3 randomLightPoint(const vec3& referencePoint, double& inversePDF){
            vec3 point;
            return point;
        }
        double areaToAnglePDFFactor(const vec3& surfacePoint, const vec3& referencePoint){
            vec3 normalVector = getNormalVector(surfacePoint);
            vec3 differenceVector = referencePoint - surfacePoint;
            vec3 vectorToPoint = normalizeVector(differenceVector);
            double PDF = dotVectors(normalVector, vectorToPoint) / differenceVector.length_squared();
            return std::max(0.0, PDF);
        }
};


class Sphere: public Object{
    public:
        double radius;
        Sphere(){}
        Sphere(vec3 _position, double _radius, Material* _material){
            position = _position;
            radius = _radius;
            material = _material;
            area = 4 * M_PI * pow(radius, 2);
        }
        Hit findClosestHit(const Ray& ray) override{

            double dotProduct = dotVectors(ray.directionVector, ray.startingPosition);
            double b = 2 * (dotProduct - dotVectors(ray.directionVector, position));
            vec3 difference_in_positions = position - ray.startingPosition;
            double c = difference_in_positions.length_squared() - pow(radius, 2);
            double distance = solveQuadratic(b, c);
            Hit hit;
            hit.objectID = 0;
            hit.distance = distance;
            return hit;
        }

        vec3 getNormalVector(const vec3& intersectionPoint) override{
            vec3 differenceVector = intersectionPoint - position;
            return normalizeVector(differenceVector);
        }

        vec3 generateRandomSurfacePoint() override{
            return sampleSpherical() * radius + position;
        }

        vec3 randomLightPoint(const vec3& referencePoint, double& inversePDF) override{
            double distance = (referencePoint - position).length();
            if (distance <= radius){
                vec3 randomPoint = generateRandomSurfacePoint();
                inversePDF = areaToAnglePDFFactor(randomPoint, referencePoint) * area;
                return randomPoint;
            }
        
        double cosThetaMax = sqrt(1 - pow(radius / distance, 2));
        inversePDF = 2 * M_PI * (1 - (cosThetaMax));

        double rand = randomUniform(0, 1);
        double cosTheta = 1 + rand * (cosThetaMax-1);
        double sinTheta = sqrt(1 - pow(cosTheta, 2));
        double cosAlpha = (pow(radius, 2) + pow(distance, 2) - pow(distance * cosTheta - sqrt(pow(radius,2) - pow(distance*sinTheta, 2)), 2)) / (2.0 * distance * radius);
        double sinAlpha = sqrt(1.0 - pow(cosAlpha, 2));
        
        vec3 xHat;
        vec3 yHat;
        vec3 zHat = getNormalVector(referencePoint);
        setPerpendicularVectors(zHat, xHat, yHat);
        double phi = randomUniform(0, 2.0*M_PI);
        vec3 randomPoint = xHat * sinAlpha * cos(phi) + yHat * sinAlpha * sin(phi) + zHat * cosAlpha;
        return randomPoint * radius + position;
        }
};


class Plane: public Object{
    public:
        vec3 v1;
        vec3 v2;
        vec3 normalVector;
        bool transparentBack;
        Plane(){}
        Plane(vec3 _position, vec3 _v1, vec3 _v2, Material* _material){
            position = _position;
            v1 = normalizeVector(_v1);
            v2 = normalizeVector(_v2);
            vec3 _normalVector = crossVectors(v1, v2);
            normalVector = normalizeVector(_normalVector);
            material = _material;
        }

        double computeDistanceInCenteredSystem(const vec3& startingPoint, const vec3& directionVector){
            double directionDotNormal = -dotVectors(directionVector, normalVector);
            if (std::abs(directionDotNormal) < constants::EPSILON){
                return -1;
            }

            double distancesToStart = dotVectors(startingPoint, normalVector);
            double distances = distancesToStart / directionDotNormal;
            return distances;
        }

        Hit findClosestHit(const Ray& ray) override{
            vec3 shiftedPoint = ray.startingPosition - position;
            double distance = computeDistanceInCenteredSystem(shiftedPoint, ray.directionVector);
            Hit hit;
            hit.objectID = 0;
            hit.distance = distance;
            return hit;
        }

        vec3 getNormalVector(const vec3& intersectionPoint) override{
            return normalVector;
        }

};


class Rectangle: public Plane{
    public:
        double L1;
        double L2;
        Rectangle(){}
        Rectangle(vec3 _position, vec3 _v1, vec3 _v2, double _L1, double _L2, Material* _material){
            position = _position;
            v1 = normalizeVector(_v1);
            v2 = normalizeVector(_v2);
            L1 = _L1;
            L2 = _L2;
            area = L1 * L2;
            vec3 _normalVector = crossVectors(v1, v2);
            normalVector = normalizeVector(_normalVector);
            material = _material;
        }

        Hit findClosestHit(const Ray& ray) override{
            Hit hit;
            hit.objectID = 0;

            vec3 shiftedPoint = ray.startingPosition - position;
            double distance = Plane::computeDistanceInCenteredSystem(shiftedPoint, ray.directionVector);
            if (distance < 0){
                hit.distance = distance;
                return hit;
            }
            double directionDotV1 = dotVectors(ray.directionVector, v1);
            double directionDotV2 = dotVectors(ray.directionVector, v2);
            double startDotV1 = dotVectors(shiftedPoint, v1);
            double startDotV2 = dotVectors(shiftedPoint, v2);

            if (std::abs(startDotV1 + directionDotV1 * distance) > L1 / 2.0 + constants::EPSILON || std::abs(startDotV2 + directionDotV2 * distance) > L2 / 2.0 + constants::EPSILON){
                distance = -1;
            }
            hit.distance = distance;
            return hit;
        }

        vec3 generateRandomSurfacePoint() override{
            double r1 = randomUniform(-L1/2, L1/2);
            double r2 = randomUniform(-L2/2, L2/2);
            return v1 * r1 + v2 * r2 + position;
        }

        vec3 randomLightPoint(const vec3& referencePoint, double& inversePDF) override{
            vec3 randomPoint = generateRandomSurfacePoint();
            inversePDF = area * areaToAnglePDFFactor(randomPoint, referencePoint);
            return randomPoint;
        }
};

Hit findClosestHit(const Ray& ray, Object** objects, const int size){
    Hit closestHit;
    closestHit.distance = -1;

    for (int i = 0; i < size; i++){
        Hit hit = objects[i] -> findClosestHit(ray);
        if (hit.distance > constants::EPSILON && (hit.distance < closestHit.distance || closestHit.distance == -1)){
            hit.intersectedObjectIndex = i;
            closestHit = hit;
        }
    }
    if (closestHit.distance < constants::EPSILON){
        return closestHit;
    }

    closestHit.intersectionPoint = ray.startingPosition + ray.directionVector * closestHit.distance;
    closestHit.normalVector = objects[closestHit.intersectedObjectIndex] -> getNormalVector(closestHit.intersectionPoint);
    closestHit.incomingVector = ray.directionVector;
    return closestHit;
 }


#endif