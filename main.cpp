#include <iostream>
#include <fstream>
#include <vector>
#include "external/glm/glm/vec3.hpp"
#include "external/glm/glm/glm.hpp"
#include "external/glm/glm/gtx/norm.hpp"

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

struct HitRecord
{
   glm::vec3 pos;
   glm::vec3 normal;
   float t;
};

class Object
{
public:
   virtual bool hit(const Ray& ray, float t_min, float t_max, HitRecord& hitRecord) = 0;
};

class Sphere : public Object
{
public:
   Sphere(glm::vec3 center, float radius)
   {
      this->center = center;
      this->radius = radius;
   }

   virtual bool hit(const Ray& ray, float t_min, float t_max, HitRecord& hitRecord) override
   {
      glm::vec3 originToCenter = ray.origin - center;
      float a = glm::length2(ray.dir);
      float half_b = glm::dot(originToCenter, ray.dir);
      float c = glm::length2(originToCenter) - radius * radius;
      float discriminant = half_b * half_b - a * c;
      float sqrtd = glm::sqrt(discriminant);

      if (discriminant < 0)
         return false;

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
      hitRecord.normal = glm::normalize(hitRecord.pos - center);

      return true;
   }

   glm::vec3 center;
   float radius;
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
         glm::ivec3 colorInt = color * glm::vec3(255.999f);
         fout << colorInt.x << " " << colorInt.y << " " << colorInt.z << " ";
      }

      fout << std::endl;
   }

   fout.close();
}

glm::vec3 rayColor(const Ray& ray)
{
   Sphere sphere = Sphere(glm::vec3(0.0f, 0.0f, -1.0f), 0.5f);

   HitRecord hitRecord;
   if (sphere.hit(ray, 0.0f, 100.0f, hitRecord))
   {
      return 0.5f * (hitRecord.normal + 1.0f); // [-1, 1] -> [0, 1]
   }

   glm::vec3 unitDir = glm::normalize(ray.dir);
   float t = 0.5f * (unitDir.y + 1.0f);

   return (1.0f - t) * glm::vec3(1.0f, 1.0f, 1.0f) + t * glm::vec3(0.5f, 0.7f, 1.0f);
}

void render(Image& image, float aspectRatio)
{
   float viewportHeight = 2.0f;
   float viewportWidth = viewportHeight * aspectRatio;
   float focalLength = 1.0f;

   glm::vec3 origin = glm::vec3(0.0f);
   glm::vec3 horizontal = glm::vec3(viewportWidth, 0.0f, 0.0f);
   glm::vec3 vertical = glm::vec3(0.0f, viewportHeight, 0.0f);
   glm::vec3 lowerLeftCorner = origin - (horizontal / 2.0f) - (vertical / 2.0f) - glm::vec3(0.0f, 0.0f, focalLength);

   for (int32_t y = image.height-1; y >= 0; y--)
   {
      for (uint32_t x = 0; x < image.width; x++)
      { 
         float u = (float)x / (image.width - 1);
         float v = (float)y / (image.height - 1);
         Ray ray = Ray(origin, lowerLeftCorner + u * horizontal + v * vertical - origin);
         glm::vec3 color = rayColor(ray);
         image.pixels[y * image.width + x] = color;
      }
   }

   std::cout << "Done" << std::endl;
}

int main(void)
{
   std::cout << "Hello world!" << std::endl;

   const float aspectRatio = 16.0f / 9.0f;
   const uint32_t width = 800;
   const uint32_t height = (uint32_t)(width / aspectRatio);

   Image image(width, height);

   render(image, aspectRatio);
   writeImage("image.ppm", image);

   return 0;
}

