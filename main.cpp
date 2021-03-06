#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include <random>
#include <thread>
#include <algorithm>
#include "external/glm/glm/vec3.hpp"
#include "external/glm/glm/glm.hpp"
#include "external/glm/glm/gtx/norm.hpp"

inline float randomFloat()
{
   static std::uniform_real_distribution<double> distribution(0.0f, 1.0f);
   static std::mt19937 generator;
   return distribution(generator);
}

inline float randomFloat(float min, float max)
{
   return min + (max - min) * randomFloat();
}

glm::vec3 randomPointInUnitSphere()
{
   while (true)
   {
      glm::vec3 point = glm::vec3(randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f));
      if (glm::length(point) < 1.0f)
         return point;
   }
}

glm::vec3 randomPointInUnitDisc()
{
   while (true)
   {
      glm::vec3 point = glm::vec3(randomFloat(-1.0f, 1.0f), randomFloat(-1.0f, 1.0f), 0.0f);
      if (glm::length2(point) < 1.0f)
         return point;
   }
}

glm::vec3 refract(const glm::vec3& uv, const glm::vec3& n, float etaiOverEtat)
{
   float cosTheta = glm::min<float>(glm::dot(-uv, n), 1.0f);
   glm::vec3 rOutPerp = etaiOverEtat * (uv + cosTheta * n);
   glm::vec3 rOutParallel = -glm::sqrt(glm::abs<float>(1.0f - glm::length2(rOutPerp))) * n;
   return rOutPerp + rOutParallel;
}

struct Image
{
   Image(uint32_t width, uint32_t height) 
   {
      this->width = width;
      this->height = height;
      pixels.resize(width * height);
   }

   std::vector<glm::vec3> pixels;
   uint32_t width;
   uint32_t height;
};

struct Ray
{
   Ray() {}
   Ray(glm::vec3 origin, glm::vec3 dir)
   {
      this->origin = origin;
      this->dir = dir;
   }

   glm::vec3 at(float t) const
   {
      return origin + t * dir;
   }

   glm::vec3 origin;
   glm::vec3 dir;
};

class Camera
{
public:
   Camera(glm::vec3 lookFrom, glm::vec3 lookAt, float verticalFov, float aspectRatio, float aperture, float focusDist)
   {
      float theta = glm::radians(verticalFov);
      float h = glm::tan(theta / 2.0f);
      float viewportHeight = 2.0f * h;
      float viewportWidth = viewportHeight * aspectRatio;

      float focalLength = 1.0f;
      glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
      w = glm::normalize(lookFrom - lookAt);
      u = glm::normalize(glm::cross(up, w));
      v = glm::cross(w, u);

      origin = lookFrom;
      horizontal = focusDist * u * viewportWidth;
      vertical = focusDist * v * viewportHeight;
      lowerLeftCorner = origin - (horizontal / 2.0f) - (vertical / 2.0f) - (focusDist * w);
      lensRadius = aperture / 2.0f;
   }

   Ray getRay(float s, float t) const
   {
      glm::vec3 rd = lensRadius * randomPointInUnitDisc();
      glm::vec3 offset = u * rd.x + v * rd.y;
      return Ray(origin + offset, lowerLeftCorner + s * horizontal + t * vertical - origin - offset);
   }

   glm::vec3 origin;
   glm::vec3 horizontal;
   glm::vec3 vertical;
   glm::vec3 lowerLeftCorner;
   glm::vec3 u, v, w;
   float lensRadius;
};

struct Material;

struct HitRecord
{
   inline void setFaceNormal(const Ray& ray, glm::vec3 outwardNormal)
   {
      frontFace = glm::dot(ray.dir, outwardNormal) < 0.0f;
      normal = frontFace ? outwardNormal : -outwardNormal;
   }

   std::shared_ptr<Material> material;
   glm::vec3 pos;
   glm::vec3 normal;
   float t;
   bool frontFace;
};

class Material
{
public:
   virtual bool scatter(const Ray& inputRay, const HitRecord& hitRecord, glm::vec3& attenuation, Ray& scatteredRay) const = 0;
};

class Lambertian : public Material
{
public:
   Lambertian(glm::vec3 color) : albedo(color) {}

   virtual bool scatter(const Ray& inputRay, const HitRecord& hitRecord, glm::vec3& attenuation, Ray& scatteredRay) const override
   {
      // Note: randomPointInUnitSphere() can be replaced by other distributions,
      // see chapter 8.5 in the tutorial.
      glm::vec3 scatterDirection = hitRecord.normal + randomPointInUnitSphere();

      if (glm::length(scatterDirection) < FLT_EPSILON)
         scatterDirection = hitRecord.normal;

      scatteredRay = Ray(hitRecord.pos, scatterDirection);
      attenuation = albedo;
      return true;
   }

   glm::vec3 albedo;
};

class Metal : public Material
{
public:
   Metal(glm::vec3 color, float f) : albedo(color), fuzz(f) {}

   virtual bool scatter(const Ray& inputRay, const HitRecord& hitRecord, glm::vec3& attenuation, Ray& scatteredRay) const override
   {
      glm::vec3 reflected = glm::reflect(glm::normalize(inputRay.dir), hitRecord.normal);
      scatteredRay = Ray(hitRecord.pos, reflected + fuzz * randomPointInUnitSphere());
      attenuation = albedo;
      return (glm::dot(scatteredRay.dir, hitRecord.normal) > 0);
   }

   glm::vec3 albedo;
   float fuzz;
};

class Dielectric : public Material
{
public:
   Dielectric(float indexOfRefraction) : ir(indexOfRefraction) {}

   virtual bool scatter(const Ray& inputRay, const HitRecord& hitRecord, glm::vec3& attenuation, Ray& scatteredRay) const override
   {
      attenuation = glm::vec3(1.0f);
      float refractionRatio = hitRecord.frontFace ? (1.0f / ir) : ir;

      glm::vec3 normalizedDirection = glm::normalize(inputRay.dir);
      float cosTheta = glm::min<float>(glm::dot(-normalizedDirection, hitRecord.normal), 1.0f);
      float sinTheta = glm::sqrt(1.0f - cosTheta * cosTheta);

      bool cannotRefract = ((refractionRatio * sinTheta) > 1.0f);
      glm::vec3 direction;

      if (cannotRefract || (calcReflectance(cosTheta, refractionRatio) > randomFloat()))
         direction = reflect(normalizedDirection, hitRecord.normal);
      else
         direction = refract(normalizedDirection, hitRecord.normal, refractionRatio);

      scatteredRay = Ray(hitRecord.pos, direction);
      return true;
   }

   float calcReflectance(float cosine, float refIdx) const
   {
      // Schlick's approximation
      float r0 = (1.0f - refIdx) / (1.0f + refIdx);
      r0 = r0 * r0;
      return r0 + (1.0f - r0) * glm::pow((1.0f - cosine), 5.0f);
   }

   float ir;
};

class Object
{
public:
   virtual bool hit(const Ray& ray, float t_min, float t_max, HitRecord& hitRecord) = 0;
};

class Sphere : public Object
{
public:
   Sphere(glm::vec3 center, float radius, std::shared_ptr<Material> material)
   {
      this->center = center;
      this->radius = radius;
      this->material = material;
   }

   virtual bool hit(const Ray& ray, float t_min, float t_max, HitRecord& hitRecord) override
   {
      glm::vec3 originToCenter = ray.origin - center;
      float a = glm::length2(ray.dir);
      float half_b = glm::dot(originToCenter, ray.dir);
      float c = glm::length2(originToCenter) - radius * radius;
      float discriminant = half_b * half_b - a * c;

      if (discriminant < 0)
         return false;

      float sqrtd = glm::sqrt(discriminant);

      // Find nearest root in the acceptable range
      float root = (-half_b - sqrtd) / a;
      if (root < t_min || root > t_max)
      {
         root = (-half_b + sqrtd) / a;
         if (root < t_min || root > t_max)
            return false;
      }

      hitRecord.t = root;
      hitRecord.pos = ray.at(hitRecord.t);
      glm::vec3 outwardNormal = (hitRecord.pos - center) / radius;
      hitRecord.setFaceNormal(ray, outwardNormal);
      hitRecord.material = material;

      return true;
   }

   std::shared_ptr<Material> material;
   glm::vec3 center;
   float radius;
};

class World
{
public:
   void addObject(std::shared_ptr<Object> object)
   {
      objects.push_back(object);
   }

   bool hit(const Ray& ray, float t_min, float t_max, HitRecord& hitRecord) const
   {
      bool hitAnything = false;
      float closestHit = t_max;

      for (const auto& object : objects)
      {
         HitRecord tempRecord;
         if (object->hit(ray, t_min, closestHit, tempRecord))
         {
            hitAnything = true;
            hitRecord = tempRecord;
            closestHit = tempRecord.t;
         }
      }

      return hitAnything;
   }
private:
   std::vector<std::shared_ptr<Object>> objects;
};

void writeImage(std::string filename, Image& image)
{
   std::ofstream fout = std::ofstream(filename);

   fout << "P3" << std::endl;
   fout << image.width << " " << image.height << std::endl;
   fout << 255 << std::endl;

   for (int32_t y = image.height-1; y >= 0; y--)
   {
      for (uint32_t x = 0; x < image.width; x++)
      {
         glm::vec3 color = image.pixels[y * image.width + x];
         glm::ivec3 colorInt = color * glm::vec3(256.0f);
         fout << colorInt.x << " " << colorInt.y << " " << colorInt.z << " ";
      }

      fout << std::endl;
   }

   fout.close();
}

glm::vec3 rayColor(const Ray& ray, const World& world, int32_t depth)
{
   HitRecord hitRecord;

   if (depth <= 0)
      return glm::vec3(0.0f);

   const float shadowAcneConstant = 0.001f;
   if (world.hit(ray, shadowAcneConstant, 100.0f, hitRecord))
   {
      Ray scatteredRay;
      glm::vec3 attenuation;

      if (hitRecord.material->scatter(ray, hitRecord, attenuation, scatteredRay))
         return attenuation * rayColor(scatteredRay, world, depth - 1);

      return glm::vec3(0.0f);
   }

   glm::vec3 unitDir = glm::normalize(ray.dir);
   float t = 0.5f * (unitDir.y + 1.0f);

   return (1.0f - t) * glm::vec3(1.0f, 1.0f, 1.0f) + t * glm::vec3(0.5f, 0.7f, 1.0f);
}

void render(Image& image, const World& world, const Camera& camera, uint32_t samplesPerPixel, int32_t maxDepth, uint32_t numThreads)
{
   std::cout << "Rendering using " << numThreads << " threads";

   auto work = [&](int32_t startRow, uint32_t numRows)
   {
      for (int32_t y = startRow; y < (startRow + numRows); y++)
      {
         for (uint32_t x = 0; x < image.width; x++)
         {
            glm::vec3 color = glm::vec3(0.0f);
            for (uint32_t s = 0; s < samplesPerPixel; s++)
            {
               float u = ((float)x + randomFloat(0.0f, 1.0f)) / (image.width - 1);
               float v = ((float)y + randomFloat(0.0f, 1.0f)) / (image.height - 1);
               Ray ray = camera.getRay(u, v);
               color += rayColor(ray, world, maxDepth);
            }

            color = color / glm::vec3(samplesPerPixel);
            color = glm::sqrt(color); // Gamma correction
            color = glm::clamp(color, glm::vec3(0.0f), glm::vec3(0.999f));
            image.pixels[y * image.width + x] = color;
         }
         std::cout << "." << std::flush;
      }
   };

   const uint32_t rowsPerThread = image.height / numThreads;
   std::vector<std::thread> workerThreads;

   for (uint32_t i = 0; i < numThreads; i++)
   {
      uint32_t rowsToProcess = rowsPerThread;
      if (i == (numThreads - 1))
         rowsToProcess = image.height - ((numThreads - 1) * rowsToProcess);

      workerThreads.push_back(std::thread(work, i * rowsPerThread, rowsToProcess));
   }

   // Wait for all workers to finish
   std::for_each(workerThreads.begin(), workerThreads.end(), [](std::thread& t) { t.join(); });

   std::cout << std::endl << "Rendering done!" << std::endl;
}

World createRandomScene()
{
   World world;

   auto groundMaterial = std::make_shared<Lambertian>(glm::vec3(0.5f, 0.5f, 0.5f));
   auto lambertianMaterial = std::make_shared<Lambertian>(glm::vec3(0.4f, 0.2f, 0.1f));
   auto dielectricMaterial = std::make_shared<Dielectric>(1.5f);
   auto metalMaterial = std::make_shared<Metal>(glm::vec3(0.7f, 0.6f, 0.5f), 0.0f);

   world.addObject(std::make_shared<Sphere>(glm::vec3(0.0f, -1000.0f, 0.0f), 1000.0f, groundMaterial));
   world.addObject(std::make_shared<Sphere>(glm::vec3(-4.0f, 1.0, 0.0), 1.0f, lambertianMaterial));
   world.addObject(std::make_shared<Sphere>(glm::vec3(0.0f, 1.0, 0.0), 1.0f, dielectricMaterial));
   world.addObject(std::make_shared<Sphere>(glm::vec3(4.0f, 1.0, 0.0), 1.0f, metalMaterial));
   
   for (int a = -11; a < 11; a++)
   {
      for (int b = -11; b < 11; b++)
      {
         float chooseMat = randomFloat();
         glm::vec3 center = glm::vec3(a + 0.9f * randomFloat(), 0.2f, b + 0.9f * randomFloat());

         if (glm::distance(center, glm::vec3(4.0f, 0.2f, 0.0f)) > 0.9f)
         {
            std::shared_ptr<Material> material;

            if (chooseMat < 0.8f)
            {
               glm::vec3 color1 = glm::vec3(randomFloat(), randomFloat(), randomFloat());
               glm::vec3 color2 = glm::vec3(randomFloat(), randomFloat(), randomFloat());
               glm::vec3 albedo = color1 * color2;
               material = std::make_shared<Lambertian>(albedo);
               world.addObject(std::make_shared<Sphere>(center, 0.2, material));
            }
            else if (chooseMat < 0.95f)
            {
               glm::vec3 albedo = glm::vec3(randomFloat(0.5f, 1.0f), randomFloat(0.5f, 1.0f), randomFloat(0.5f, 1.0f));
               float fuzz = randomFloat(0.0f, 0.5f);
               material = std::make_shared<Metal>(albedo, fuzz);
               world.addObject(std::make_shared<Sphere>(center, 0.2, material));
            }
            else
            {
               material = std::make_shared<Dielectric>(1.5f);
               world.addObject(std::make_shared<Sphere>(center, 0.2, material));
            }
         }
      }
   }

   return world;
}

int main(void)
{
   const float aspectRatio = 3.0f / 2.0f;
   const uint32_t width = 1200;
   const uint32_t height = (uint32_t)(width / aspectRatio);
   const uint32_t samplesPerPixel = 500;
   const int32_t maxDepth = 50;
   const uint32_t numThreads = 16;

   Image image(width, height);
   Camera camera = Camera(glm::vec3(13.0f, 2.0f, 3.0f), glm::vec3(0.0f), 20.0f, aspectRatio, 0.1f, 10.0f);
   World world = createRandomScene();

   render(image, world, camera, samplesPerPixel, maxDepth, numThreads);
   writeImage("image.ppm", image);

   return 0;
}

