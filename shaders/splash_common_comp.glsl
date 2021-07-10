struct DispatchCommand {
    uint x;
    uint y;
    uint z;
};

struct DrawCommand {
    uint    vertex_count;
    uint    instance_count;
    uint    first_vertex;
    uint    first_instance;
};

struct SplashHint {
    vec3 position;
    float lifetime;
    vec3 normal;
    float initial_speed;
};

const float kMaxLifetime = 0.12;
