#include <iostream>
#include <fstream>
#include <chrono>
#include "vec3.h"
#include "objects.h"
#include "camera.h"
#include "utils.h"
#include "constants.h"


DiffuseMaterial whiteDiffuseMaterial = DiffuseMaterial(WHITE);
DiffuseMaterial redDiffuseMaterial = DiffuseMaterial(RED);
DiffuseMaterial greenDiffuseMaterial = DiffuseMaterial(GREEN);
ReflectiveMaterial blueReflectiveMaterial = ReflectiveMaterial(BLUE);
ReflectiveMaterial whiteReflectiveMaterial = ReflectiveMaterial(WHITE);
DiffuseMaterial ball2Material = DiffuseMaterial(WHITE, 1, 1.5);
DiffuseMaterial lightSourceMaterial = DiffuseMaterial(WHITE, 0.8, 1, 1, WARM_WHITE, 10);

Plane thisFloor = Plane(vec3(0,-0.35,0), vec3(1,0,0), vec3(0,0,-1), &whiteDiffuseMaterial);
Plane  frontWall = Plane(vec3(0,0,-0.35), vec3(1,0,0), vec3(0,1,0), &whiteDiffuseMaterial);
Plane leftWall = Plane(vec3(1,0,0), vec3(0,0,1), vec3(0,1,0), &redDiffuseMaterial);
Plane rightWall = Plane(vec3(-1,0,0), vec3(0,1,0), vec3(0,0,1), &greenDiffuseMaterial);
Plane roof = Plane(vec3(0,1.2,0), vec3(0,0,-1), vec3(1,0,0), &whiteDiffuseMaterial);
Plane backWall = Plane(vec3(0,0,3.5), vec3(0,1,0), vec3(1,0,0), &whiteDiffuseMaterial);

Sphere ball1 = Sphere(vec3(0.35,0,0), 0.35, &blueReflectiveMaterial);
Sphere ball2 = Sphere(vec3(-0.45,0,0.6), 0.35, &ball2Material);

Rectangle lightSource = Rectangle(vec3(0, 1.199, 1), vec3(0,0,-1), vec3(1,0,0), 1, 1, &lightSourceMaterial);

Object* objectPtrList[] = {&thisFloor, &roof, &frontWall, &backWall, &rightWall, &leftWall, &ball1, &ball2, &lightSource};
Object* lightsourcePtrList[] = {&lightSource};

vec3 cameraPosition = vec3(0, 1, 3);
vec3 viewingDirection = vec3(0.0, -0.3, -1);
vec3 screenYVector = vec3(0, 1, 0);
Camera camera = Camera(cameraPosition, viewingDirection, screenYVector);


vec3 raytrace(Ray ray);


vec3 directLighting(const Hit& hit){
    int numLightSources = sizeof(lightsourcePtrList) / sizeof(Object*);
    int randomIndex = randomInt(0, numLightSources);
    int lightIndex;
    for (int i = 0; i < sizeof(objectPtrList) / sizeof(Object*); i++){
        if (objectPtrList[i] == lightsourcePtrList[randomIndex]){
            lightIndex = i;
            break;
        }
    }
    double inversePDF;
    vec3 randomPoint = lightsourcePtrList[randomIndex] -> randomLightPoint(hit.intersectionPoint, inversePDF);

    vec3 vectorTowardsLight = randomPoint - hit.intersectionPoint;
    double distanceToLight = vectorTowardsLight.length();
    vectorTowardsLight = normalizeVector(vectorTowardsLight);

    Ray lightRay;
    lightRay.startingPosition = hit.intersectionPoint;
    lightRay.directionVector =  vectorTowardsLight;

    int numberOfObjects = sizeof(objectPtrList) / sizeof(Object*);
    Hit lightHit = findClosestHit(lightRay, objectPtrList, numberOfObjects);

    bool sameDistance = std::abs(distanceToLight - lightHit.distance) <= constants::EPSILON;
    if (lightHit.intersectedObjectIndex != lightIndex || hit.intersectedObjectIndex == lightIndex || !sameDistance){
        return BLACK;
    }

    vec3 brdfMultiplier = objectPtrList[hit.intersectedObjectIndex] -> material -> eval();
    vec3 lightEmitance = lightsourcePtrList[randomIndex] -> material -> getLightEmittance();
    double cosine = dotVectors(hit.normalVector, vectorTowardsLight);
    cosine = std::max(0.0, cosine);

    return brdfMultiplier * cosine * lightEmitance * inversePDF * (double) numLightSources;
}


vec3 raytrace(Ray ray){
    vec3 color = vec3(0,0,0);
    vec3 brdfAccumulator = vec3(1,1,1);
    vec3 throughput = vec3(1,1,1);
    int forceRecusionLimit = 3;
    int numberOfObjects = sizeof(objectPtrList) / sizeof(Object*);
    double randomThreshold = 1;
    for (int depth = 0; depth <= constants::maxRecursionDepth; depth++){

        Hit rayHit = findClosestHit(ray, objectPtrList, numberOfObjects);
        if (rayHit.distance <= constants::EPSILON){
            break;
        }

        if (!constants::enableNextEventEstimation || depth == 0 || ray.specular){
            vec3 lightEmitance = objectPtrList[rayHit.intersectedObjectIndex] -> material -> getLightEmittance();
            color += lightEmitance * brdfAccumulator * (dotVectors(ray.directionVector, rayHit.normalVector) < 0);
        }

        bool allowRecursion;
        double randomThreshold;

        if (depth < forceRecusionLimit){
            randomThreshold = 1;
            allowRecursion = true;
        }
        else{
            randomThreshold = std::min(throughput.max(), 0.9);
            double randomValue = randomUniform(0, 1);
            allowRecursion = randomValue < randomThreshold;
        }
        
        if (!allowRecursion){
            break;
        }   

        if (constants::enableNextEventEstimation){
            vec3 direct = directLighting(rayHit);
            color += direct * brdfAccumulator / randomThreshold;
        }

        brdfData brdfResult = objectPtrList[rayHit.intersectedObjectIndex] -> material -> sample(rayHit, objectPtrList, numberOfObjects);

        throughput *= brdfResult.brdfMultiplier / randomThreshold;
        ray.startingPosition = rayHit.intersectionPoint;
        ray.directionVector = brdfResult.outgoingVector;
        ray.specular = brdfResult.specular;

        brdfAccumulator *= brdfResult.brdfMultiplier / randomThreshold;
    }
    return color;

 }


vec3 computePixelColor(const int x, const int y){
    vec3 pixelColor = vec3(0,0,0);
    Ray ray;
    ray.startingPosition = cameraPosition;
    for (int i = 0; i < constants::samplesPerPixel; i++){
        double x_offset = randomNormal() / 2.0;
        double y_offset = randomNormal() / 2.0;
        ray.directionVector = camera.getStartingDirections(x + x_offset, y + y_offset);
        vec3 sampledColor = raytrace(ray);
        pixelColor += sampledColor;    
    }

    return pixelColor / (double) constants::samplesPerPixel;
}


void printPixelColor(vec3 rgb, std::ofstream& file){
    int r = int(rgb[0] * (double) 255);
    int g = int(rgb[1] * (double) 255);
    int b = int(rgb[2] * (double) 255);
    file << r << ' ' << g << ' ' << b << '\n';
}


void printProgress(double progress){
    if (progress < 1.0) {
        int barWidth = 70;

        std::clog << "[";
        int pos = barWidth * progress;
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::clog << "=";
            else if (i == pos) std::clog << ">";
            else std::clog << " ";
        }
        std::clog << "] " << int(progress * 100.0) << " %\r";
    }
}


int main() {
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    std::ofstream dataFile;
    dataFile.open("./temp/result_data.txt");
    
    dataFile << "SIZE:" << constants::WIDTH << ' ' << constants::HEIGHT << "\n";
    int maxIters = 0;
    for (int y = constants::HEIGHT-1; y >= 0; y--) {
        double progress = double(constants::HEIGHT - y) / (double) constants::HEIGHT;
        printProgress(progress);
        for (int x = constants::WIDTH-1; x >= 0; x--) {
            vec3 rgb = computePixelColor(x, y);
            printPixelColor(colorClip(rgb), dataFile);
        }
    }
    dataFile.close();
    std::clog << std::endl;
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    std::clog << "Time taken: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;

    return 0;
}