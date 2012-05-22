#ifndef Magnum_Mesh_h
#define Magnum_Mesh_h
/*
    Copyright © 2010, 2011, 2012 Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Magnum.

    Magnum is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Magnum is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

/** @file
 * @brief Class Magnum::Mesh
 */

#include <map>
#include <vector>
#include <set>

#include "Magnum.h"
#include "TypeTraits.h"

namespace Magnum {

class Buffer;

/**
@brief Base class for managing non-indexed meshes

@todo Support for normalized values (e.g. for color as char[4] passed to
     shader as floating-point vec4)

@requires_gl30 Extension <tt>APPLE_vertex_array_object</tt>
@requires_gl30 Extension <tt>EXT_gpu_shader4</tt> (for unsigned integer attributes)

@todo Support for provoking vertex (OpenGL 3.2, ARB_provoking_vertex)
@todo Support for packed unsigned integer types for attributes (OpenGL 3.3, ARB_vertex_type_2_10_10_10_rev)
@todo Support for fixed precision type for attributes (OpenGL 4.1, ARB_ES2_compatibility)
@todo Support for double type for attributes (OpenGL 4.1, ARB_vertex_attrib_64bit)
@todo Support for indirect draw buffer (OpenGL 4.0, ARB_draw_indirect)
 */
class MAGNUM_EXPORT Mesh {
    Mesh(const Mesh& other) = delete;
    Mesh(Mesh&& other) = delete;
    Mesh& operator=(const Mesh& other) = delete;
    Mesh& operator=(Mesh&& other) = delete;

    public:
        /**
         * @brief Polygon mode
         *
         * @see setPolygonMode()
         */
        enum class PolygonMode: GLenum {
            /**
             * Interior of the polygon is filled.
             */
            Fill = GL_FILL,

            /**
             * Boundary edges are filled. See also setLineWidth().
             */
            Line = GL_LINE,

            /**
             * Starts of boundary edges are drawn as points. See also
             * setPointSize().
             */
            Point = GL_POINT
        };

        /**
         * @brief Primitive type
         *
         * @see primitive(), setPrimitive()
         */
        enum class Primitive: GLenum {
            /**
             * Single points
             */
            Points = GL_POINTS,

            /**
             * Each pair of vertices defines a single line, lines aren't
             * connected together.
             */
            Lines = GL_LINES,

            /**
             * Polyline
             */
            LineStrip = GL_LINE_STRIP,

            /**
             * Polyline, last vertex is connected to first.
             */
            LineLoop = GL_LINE_LOOP,

            /**
             * Each three vertices define one triangle.
             */
            Triangles = GL_TRIANGLES,

            /**
             * First three vertices define first triangle, each following
             * vertex defines another triangle.
             */
            TriangleStrip = GL_TRIANGLE_STRIP,

            /**
             * First vertex is center, each following vertex is connected to
             * previous and center vertex.
             */
            TriangleFan = GL_TRIANGLE_FAN
        };

        /**
         * @brief Set polygon drawing mode
         *
         * Initial value is PolygonMode::Fill.
         */
        inline static void setPolygonMode(PolygonMode mode) {
            glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(mode));
        }

        /**
         * @brief Set line width
         *
         * Initial value is 1.
         */
        inline static void setLineWidth(GLfloat width) {
            glLineWidth(width);
        }

        /**
         * @brief Set point size
         *
         * @see setProgramPointSize()
         */
        inline static void setPointSize(GLfloat size) {
            glPointSize(size);
        }

        /**
         * @brief Enable/disable programmable point size
         *
         * If enabled, the point size is taken from vertex/geometry shader
         * builtin `gl_PointSize`.
         * @see setPointSize()
         */
        inline static void setProgramPointSize(bool enabled) {
            enabled ? glEnable(GL_PROGRAM_POINT_SIZE) : glDisable(GL_PROGRAM_POINT_SIZE);
        }

        /**
         * @brief Implicit constructor
         * @param primitive     Primitive type
         *
         * Allows creating the object without knowing anything about mesh
         * data. Note that you have to call setVertexCount() manually for mesh
         * to draw properly.
         */
        inline Mesh(Primitive primitive = Primitive::Triangles): _primitive(primitive), _vertexCount(0), finalized(false) {
            glGenVertexArrays(1, &vao);
        }

        /**
         * @brief Constructor
         * @param primitive     Primitive type
         * @param vertexCount   Vertex count
         */
        inline Mesh(Primitive primitive, GLsizei vertexCount): _primitive(primitive), _vertexCount(vertexCount), finalized(false) {
            glGenVertexArrays(1, &vao);
        }

        /**
         * @brief Destructor
         *
         * Deletes all associated buffers.
         */
        virtual ~Mesh();

        /**
         * @brief Whether the mesh is finalized
         *
         * When the mesh is finalized, no new attributes can be bound.
         */
        inline bool isFinalized() const { return finalized; }

        /** @brief Primitive type */
        inline Primitive primitive() const { return _primitive; }

        /** @brief Set primitive type */
        inline void setPrimitive(Primitive primitive) { _primitive = primitive; }

        /** @brief Vertex count */
        inline GLsizei vertexCount() const { return _vertexCount; }

        /**
         * @brief Set vertex count
         *
         * This forces recalculation of attribute positions upon next drawing.
         */
        inline void setVertexCount(GLsizei vertexCount) {
            _vertexCount = vertexCount;
            finalized = false;
        }

        /**
         * @brief Add buffer
         * @param interleaved   If storing more than one attribute data in the
         *      buffer, the data of one attribute can be either kept together
         *      or interleaved with data for another attributes, so data for
         *      every vertex will be in one continuous place.
         *
         * Adds new buffer to the mesh. The buffer can be then filled with
         * Buffer::setData(). See also isInterleaved().
         *
         * @todo Move interleaveability to Buffer itself?
         */
        Buffer* addBuffer(bool interleaved);

        /**
         * @brief Whether given buffer is interleaved
         * @return True if the buffer belongs to the mesh and the buffer is
         *      interleaved, false otherwise.
         *
         * See also addBuffer().
         */
        inline bool isInterleaved(Buffer* buffer) const {
            auto found = _buffers.find(buffer);
            return found != _buffers.end() && found->second.first;
        }

        /**
         * @brief Bind attribute
         * @tparam attribute    Attribute, defined in the shader
         * @param buffer        Buffer where bind the attribute to (pointer
         *      returned by addBuffer())
         *
         * Binds attribute of given type with given buffer. If the attribute is
         * already bound, given buffer isn't managed with this mesh (wasn't
         * initialized with addBuffer) or the mesh was already drawn, the
         * function does nothing.
         */
        template<class Attribute> inline void bindAttribute(Buffer* buffer) {
            bindAttribute(buffer, Attribute::Location, TypeTraits<typename Attribute::Type>::count(), TypeTraits<typename Attribute::Type>::type());
        }

        /**
         * @brief Draw the mesh
         *
         * Binds attributes to buffers and draws the mesh. Expects an active
         * shader with all uniforms set.
         */
        virtual void draw();

    protected:
        /** @brief Unbind any vertex array object */
        inline static void unbind() { glBindVertexArray(0); }

        /** @brief Bind vertex array object of current mesh */
        inline void bind() { glBindVertexArray(vao); }

        /**
         * @brief Finalize the mesh
         *
         * Computes location and stride of each attribute in its buffer. After
         * this function is called, no new attribute can be bound.
         */
        MAGNUM_LOCAL void finalize();

    private:
        /** @brief Vertex attribute */
        struct MAGNUM_LOCAL Attribute {
            GLuint attribute;           /**< @brief %Attribute ID */
            GLint size;                 /**< @brief How many items of `type` are in the attribute */
            Type type;                  /**< @brief %Attribute item type */
            GLsizei stride;             /**< @brief Distance of two adjacent attributes of this type in interleaved buffer */
            const GLvoid* pointer;      /**< @brief Pointer to first attribute of this type in the buffer */
        };

        GLuint vao;
        Primitive _primitive;
        GLsizei _vertexCount;
        bool finalized;

        /**
         * @brief Buffers with their attributes
         *
         * Map of associated buffers, evey buffer has:
         * - boolean value which signalizes whether the buffer is interleaved
         * - list of bound attributes
         */
        std::map<Buffer*, std::pair<bool, std::vector<Attribute> > > _buffers;

        /**
         * @brief List of all bound attributes
         *
         * List of all bound attributes bound with bindAttribute().
         */
        std::set<GLuint> _attributes;

        MAGNUM_EXPORT void bindAttribute(Buffer* buffer, GLuint attribute, GLint size, Type type);
};

}

#endif
