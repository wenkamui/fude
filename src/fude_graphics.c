#include "fude.h"

#include "glad/glad.h"
#include <stddef.h>

fude_result _fude_init_renderer(fude* app, const fude_config* config)
{
    (void)config;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenVertexArrays(1, &app->renderer.id);
    glBindVertexArray(app->renderer.id);

    glGenBuffers(1, &app->renderer.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, app->renderer.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fude_vertex)*FUDE_RENDERER_MAXIMUM_VERTICES, 
            app->renderer.vertices.data, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 
            sizeof(fude_vertex), (GLvoid*)offsetof(fude_vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 
            sizeof(fude_vertex), (GLvoid*)offsetof(fude_vertex, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 
            sizeof(fude_vertex), (GLvoid*)offsetof(fude_vertex, tex_coords));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 
            sizeof(fude_vertex), (GLvoid*)offsetof(fude_vertex, tex_index));

    glGenBuffers(1, &app->renderer.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->renderer.ibo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uint32_t)*FUDE_RENDERER_MAXIMUM_INDICIES, 
            app->renderer.indices.data, GL_DYNAMIC_DRAW);

    return FUDE_OK;
}

void f_begin(fude* f, fude_draw_mode mode)
{
    f->renderer.working.count = 0;
    f->renderer.working.mode = mode;
}

void f_end(fude* app)
{
    if(app->renderer.working.mode == FUDE_MODE_QUADS && app->renderer.working.count >= 4) {
        uint32_t nquads = app->renderer.working.count / 4;
        for(uint32_t i = 0; i < 6*nquads; i+=6) {
            app->renderer.indices.data[i + 0] = app->renderer.vertices.count + 0;
            app->renderer.indices.data[i + 1] = app->renderer.vertices.count + 1;
            app->renderer.indices.data[i + 2] = app->renderer.vertices.count + 2;
            app->renderer.indices.data[i + 3] = app->renderer.vertices.count + 2;
            app->renderer.indices.data[i + 4] = app->renderer.vertices.count + 3;
            app->renderer.indices.data[i + 5] = app->renderer.vertices.count + 0;
            app->renderer.vertices.count += 4;
            app->renderer.indices.count += 4;
        }
    }

    for(uint32_t i = 0 ; i < 3; i += 3) {
        app->renderer.indices.data[i + 0] = app->renderer.vertices.count + 0;
        app->renderer.indices.data[i + 1] = app->renderer.vertices.count + 1;
        app->renderer.indices.data[i + 2] = app->renderer.vertices.count + 2;
        app->renderer.vertices.count += 3;
        app->renderer.indices.count += 3;
    }
}

void f_color4f(fude* app, float r, float g, float b, float a)
{
    app->renderer.working.vertex.color.r = r;
    app->renderer.working.vertex.color.g = g;
    app->renderer.working.vertex.color.b = b;
    app->renderer.working.vertex.color.a = a;
}

void f_color(fude* app, fude_color color)
{
    f_color4f(app, (float)color.r/255, (float)color.g/255, (float)color.b/255, (float)color.a/255);
}

void f_vertex3f(fude* app, float x, float y, float z)
{
    app->renderer.working.vertex.position.x = x;
    app->renderer.working.vertex.position.y = y;
    app->renderer.working.vertex.position.z = z;

    app->renderer.vertices.data[app->renderer.vertices.count] = app->renderer.working.vertex;
    app->renderer.working.count += 1;
}

void f_vertex2f(fude* app, float x, float y)
{
    f_vertex3f(app, x, y, 0.0f); // TODO: Make the Z coordinate dynamic
}

void f_flush(fude* app)
{
    // sync the data
    glBindBuffer(GL_ARRAY_BUFFER, app->renderer.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, app->renderer.vertices.count*sizeof(fude_vertex), 
            app->renderer.vertices.data);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->renderer.ibo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, app->renderer.indices.count*sizeof(uint32_t), 
            app->renderer.indices.data);

    // make draw call
    glUseProgram(app->renderer.shader.id);
    glBindVertexArray(app->renderer.id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->renderer.ibo);
    glDrawElements(GL_TRIANGLES, app->renderer.indices.count, GL_UNSIGNED_INT, 0);

    // clear the data
    f_memzero(app->renderer.vertices.data, sizeof(fude_vertex)*app->renderer.vertices.count);
    f_memzero(app->renderer.indices.data, sizeof(uint32_t)*app->renderer.indices.count);
    app->renderer.vertices.count = 0;
    app->renderer.indices.count = 0;
    app->renderer.shader = app->renderer.default_shader;

    for(uint32_t i = 0; i < FUDE_RENDERER_MAXIMUM_TEXTURES; ++i)
        app->renderer.textures[i] = app->renderer.default_texture;
}

void f_use_shader(fude* f, fude_shader shader)
{
    f->renderer.shader = shader;
}

fude_result f_load_shader(fude_shader* shader, const char* vert_src, const char* frag_src)
{
    if(!shader) return FUDE_INVALID_ARGUMENTS_ERROR;
    if(!vert_src) return FUDE_INVALID_ARGUMENTS_ERROR;
    if(!frag_src) return FUDE_INVALID_ARGUMENTS_ERROR;

    uint32_t vert_module, frag_module;
    GLchar info_log[512] = {0};
    int success;
    vert_module = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_module, 1, (const GLchar* const*)&vert_src, NULL);
    glCompileShader(vert_module);
    glGetShaderiv(vert_module, GL_COMPILE_STATUS, &success);
    f_trace_log(FUDE_LOG_INFO, "success = %d", success);
    if(!success) {
        glGetShaderInfoLog(vert_module, sizeof(info_log), NULL, info_log);
        f_trace_log(FUDE_LOG_ERROR, "VERTEX SHADER: %s\n\t", info_log); // TODO: Error
        return FUDE_SHADER_CREATION_ERROR;
    }

    frag_module = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_module, 1, (const GLchar* const*)&frag_src, NULL);
    glCompileShader(frag_module);
    glGetShaderiv(frag_module, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(frag_module, sizeof(info_log), NULL, info_log);
        f_trace_log(FUDE_LOG_ERROR, "FRAGMENT SHADER: %s\n", info_log);
        return FUDE_SHADER_CREATION_ERROR;
    }

    shader->id = glCreateProgram();
    glAttachShader(shader->id, vert_module);
    glAttachShader(shader->id, frag_module);
    glLinkProgram(shader->id);
    glGetProgramiv(shader->id, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(shader->id, sizeof(info_log), NULL, info_log);
        f_trace_log(FUDE_LOG_ERROR, "SHADER PROGRAM: %s\n", info_log);
        return FUDE_SHADER_CREATION_ERROR;
    }

    glDeleteShader(vert_module);
    glDeleteShader(frag_module);

    glUseProgram(shader->id);

    fude_result result = FUDE_OK;

    result = f_get_shader_uniform_location(*shader, &shader->uniform_loc[FUDE_UNIFORM_TEXTURE_SAMPLERS_LOC],
                FUDE_TEXTURE_SAMPLER_UNIFORM_NAME);
    if(result != FUDE_OK) {
        return result;
    }

    result = f_get_shader_uniform_location(*shader, &shader->uniform_loc[FUDE_UNIFORM_MATRIX_MVP_LOC],
                FUDE_MATRIX_MVP_UNIFORM_NAME);
    if(result != FUDE_OK) {
        return result;
    }

    result = f_get_shader_uniform_location(*shader, &shader->uniform_loc[FUDE_UNIFORM_MATRIX_PROJECTION_LOC],
                FUDE_MATRIX_PROJECTION_UNIFORM_NAME);
    if(result != FUDE_OK) {
        return result;
    }

    result = f_get_shader_uniform_location(*shader, &shader->uniform_loc[FUDE_UNIFORM_MATRIX_VIEW_LOC],
                FUDE_MATRIX_VIEW_UNIFORM_NAME);
    if(result != FUDE_OK) {
        return result;
    }

    result = f_get_shader_uniform_location(*shader, &shader->uniform_loc[FUDE_UNIFORM_MATRIX_MODEL_LOC],
                FUDE_MATRIX_MODEL_UNIFORM_NAME);
    if(result != FUDE_OK) {
        return result;
    }

    return FUDE_OK;
}

fude_result f_load_shader_from_file(fude_shader* shader, const char* vert_path, const char* frag_path)
{
    char* vert_src = f_load_file_data(vert_path, NULL);
    char* frag_src = f_load_file_data(frag_path, NULL);
    fude_result result = f_load_shader(shader, vert_path, frag_path);

    if(result != FUDE_OK) {
        return result;
    }

    f_unload_file_data(vert_src);
    f_unload_file_data(frag_src);
    return FUDE_OK;
}

void f_unload_shader(fude_shader shader)
{
    glDeleteProgram(shader.id);
}

fude_result f_get_shader_uniform_location(fude_shader shader, int* location, const char* name)
{
    if(!location) return FUDE_INVALID_ARGUMENTS_ERROR;
    glUseProgram(shader.id);
    int _location = glGetUniformLocation(shader.id, name);
    if(_location < 0) {
        f_trace_log(FUDE_LOG_ERROR, "Uniform %s not found in shader program", name);
        return FUDE_UNIFORM_LOCATION_NOT_FOUND_ERROR;
    }
    return FUDE_OK;
}