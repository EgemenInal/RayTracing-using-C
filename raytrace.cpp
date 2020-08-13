#define _USE_MATH_DEFINES
#include <cmath>
#include <limits>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include "geometry.h"
using namespace std;

struct Light {
    Light(const Vec3f &p, const float i) : light_pos(p), light_intensity(i) {}
    Vec3f light_pos;
    float light_intensity;
};

struct Material {
    Material(const float r, const Vec4f &a, const Vec3f &color, const float spec) : 
        refractive_index(r), 
        brightness(a), 
        diffuse(color), 
        specular_exponent(spec) {}
    Material() : refractive_index(1), brightness(1,0,0,0), diffuse(), specular_exponent() {}
    float refractive_index;
    Vec4f brightness;
    Vec3f diffuse;
    float specular_exponent;
};

struct Sphere {
    Vec3f center;
    float radius;
    Material material;

    Sphere(const Vec3f &c, const float r, const Material &m) : center(c), radius(r), material(m) {}

    bool ray_intersect(const Vec3f &orig, const Vec3f &dir, float &di1) const {
        Vec3f hyp = center - orig;
        float rayline = hyp*dir;
	if(rayline <0){//sphere behind light
		return false;
	}
        float prj = hyp*hyp - rayline*rayline;//projection of center on the line
	/*if(prj <= radius){// hmmm donuts 
		return false;
	}*/
        float botl = sqrtf(radius*radius - prj);
	if(sqrt(hyp*hyp)>radius){
		 di1   = rayline - botl;
	}
	else{
		di1   = rayline + botl;
	}
	if(di1>0){return true;}
	else{return false;}

    }
};

Vec3f reflect(const Vec3f &I, const Vec3f &N) {
    float cosa = -I*N;//unit vectors |a|=1 |b|=1 formula is cosa =a*b/(|a|*|b|)  
    return I + N*2*(cosa); //formula
}

Vec3f refract(const Vec3f &I, const Vec3f &N, const float eta_t, const float eta_i=1.f) { // Snell's law
    float cosa = -I*N;//unit vectors |a|=1 |b|=1 formula is cosa =a*b/(|a|*|b|)  
    if (cosa<0) return refract(I, -N, eta_i, eta_t); // if the ray comes from the inside the object, swap the air and the media
    float eta = eta_i / eta_t;
    float sin2a = 1 - cosa*cosa;
    float sinb = eta* sqrt(sin2a);
    float cosb = sqrt(1-sinb*sinb);
    return I*eta+N*(eta*cosa-cosb);
}

bool scene_intersect(const Vec3f &orig, const Vec3f &dir, const vector<Sphere> &spheres, Vec3f &hit, Vec3f &N, Material &material) {
    float spheres_dist = 20000;
    for (size_t i=0; i < spheres.size(); i++) {
        float dist_i;
        if (spheres[i].ray_intersect(orig, dir, dist_i) && dist_i < spheres_dist) {
            spheres_dist = dist_i;
            hit = orig + dir*dist_i;
            N = (hit - spheres[i].center).normalize();
            material = spheres[i].material;
        }
    }
 float surface = 20000;
    if (fabs(dir.y)>1e-3)  {
        float d = -(orig.y+4)/dir.y; // the checkerboard plane has equation y = -4
        Vec3f point = orig + dir*d;
        if (d>0 && fabs(point.x)<10 && point.z<-10 && point.z>-30 && d<spheres_dist) {
            surface = d;
            hit = point;
            N = Vec3f(0,1,0);
            material.diffuse =  Vec3f(0.4, .4, .4)*.3;

        }

    }
    return min(spheres_dist, surface)<1000;
}

Vec3f cast_ray(const Vec3f &orig, const Vec3f &dir, const vector<Sphere> &spheres, const vector<Light> &lights, size_t bounce=0) {
    Vec3f point, N;
    Material material;

    if (bounce>3 || !scene_intersect(orig, dir, spheres, point, N, material)) {//bounce of relections
        return Vec3f(0.9, 0.9, 0.9); // background color
    }

    Vec3f reflect_dir = reflect(dir, N).normalize();

    Vec3f reflect_orig = point;
    Vec3f reflect_color = cast_ray(reflect_orig, reflect_dir, spheres, lights, bounce + 1);

    Vec3f refract_dir = refract(dir, N, material.refractive_index).normalize();
    Vec3f refract_orig;
	  if(refract_dir*N < 0) 
		{ refract_orig=point - N;}
	  else  
		{refract_orig=point;}//light from inside

    Vec3f refract_color = cast_ray(refract_orig, refract_dir, spheres, lights, bounce + 1);

    float diffuse_light_intensity = 0, specular_light_intensity = 0;
    for (size_t i=0; i<lights.size(); i++) {
        Vec3f light_dir      = (lights[i].light_pos - point).normalize();

        Vec3f shadow_orig;
	
 	if(light_dir*N < 0){
		shadow_orig= point;}

	else{//shadow from outside
		shadow_orig= point + N;
	} // checking if the point lies in the shadow of the lights[i]

        Vec3f shadow_pt, shadow_N;
        Material tmpmaterial;
        if (scene_intersect(shadow_orig, light_dir, spheres, shadow_pt, shadow_N, tmpmaterial))
            continue;

        diffuse_light_intensity  += N*light_dir*lights[i].light_intensity ;
        specular_light_intensity += powf(reflect(light_dir, N)*dir, material.specular_exponent)*lights[i].light_intensity;
    }
    return material.diffuse * diffuse_light_intensity * material.brightness[0] + Vec3f(1., 1., 1.)*specular_light_intensity 
    * material.brightness[1] + reflect_color*material.brightness[2] + refract_color*material.brightness[3];
}

void render(const vector<Sphere> &spheres, const vector<Light> &lights) {
    const int   width    = 1024;
    const int   height   = 768;
    //const float fov      = M_PI/3.;
    vector<Vec3f> framebuffer(width*height);

    for (int j = 0; j<height; j++) { // actual rendering loop
        for (int i = 0; i<width; i++) {
            float dir_x =  i -  width/2.;
            float dir_y =  -j + height/2.;    // can be used for flipping
            float dir_z = -700;
            framebuffer[i+j*width] = cast_ray(Vec3f(0,0,1).normalize(), Vec3f(dir_x, dir_y, dir_z).normalize(), spheres, lights);
        }
    }

    ofstream ofs; // save the framebuffer to file
    ofs.open("./out.ppm",ios::binary);
    ofs << "P6\n" << width << " " << height << "\n255\n";
    for (size_t i = 0; i < height*width; ++i) {
        Vec3f &c = framebuffer[i];
        float max = std::max(c[0], std::max(c[1], c[2]));
        if (max>1) c = c*(1./max);
        for (size_t j = 0; j<3; j++) {
            ofs << (char)(255 * std::max(0.f, min(1.f, framebuffer[i][j])));
        }
    }
    ofs.close();
}

int main() {
    Material      green1(1.0, Vec4f(0.6,  0.3, 0.1, 0.0), Vec3f(0.1, 0.4, 0.1),   50.);
    Material      green2(1.0, Vec4f(0.8,  .5, 0.2, 0.), Vec3f(0.0, 0.3, 0.0),   10.);
    Material      glass(2.0, Vec4f(0.0,  0.5, 0.1, 0.8), Vec3f(0.6, 0.7, 0.8),  125.);
    Material 	  red(1.0, Vec4f(0.9,  0.1, 0.0, 0.0), Vec3f(0.5, 0, 0),   10.);
    Material     mirror(1.0, Vec4f(0.0, 10.0, 0.8, 0.0), Vec3f(1.0, 1.0, 1.0), 1425.);

    vector<Sphere> spheres;
    spheres.push_back(Sphere(Vec3f(-7,    0,   -20), 3,      green1));
    spheres.push_back(Sphere(Vec3f(1,    -1,   -10), 1,      green2));
    spheres.push_back(Sphere(Vec3f( 3, 3, -12), 2,      glass));
    spheres.push_back(Sphere(Vec3f( 1.5, -0.5, -18), 4, red));
    spheres.push_back(Sphere(Vec3f( -4,    4,   -14), 3,     mirror));

    vector<Light>  lights;
    lights.push_back(Light(Vec3f(-20, 20,  20), 1.5));
    lights.push_back(Light(Vec3f( 30, 50, -25), 1.8));
    lights.push_back(Light(Vec3f( 30, 20,  30), 1.7));
    clock_t begin = clock();
    render(spheres, lights);
    clock_t end = clock();
double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
cout<<time_spent<<endl;
    return 0;
}

