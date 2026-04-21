#pragma once
// ============================================================
// bloom_pipeline.hpp — Multi-level bloom with RAII
// ============================================================

#include "platform.hpp"
#include "framebuffer.hpp"
#include "gl_types.hpp"
#include "shader_sources.hpp"
#include "config.hpp"

#include <iostream>

struct BloomPipeline {
    static constexpr int NUM_MIPS = 4;

    Framebuffer sceneFBO;
    Framebuffer thresholdFBO;
    Framebuffer mipFBOs[NUM_MIPS];
    Framebuffer blurTempFBO[NUM_MIPS];

    GLProgram thresholdProg;
    GLProgram blurProg;
    GLProgram compositeProg;

    BloomPipeline() = default;
    ~BloomPipeline() = default;

    // Non-copyable (Framebuffer & GLProgram are move-only)
    BloomPipeline(const BloomPipeline&) = delete;
    BloomPipeline& operator=(const BloomPipeline&) = delete;

    // Build shaders & FBOs.  Returns false on compilation failure.
    bool init(int w, int h) {
        if (!thresholdProg.build(vertexShaderSrc, luminosityHighpassSrc)) return false;
        if (!blurProg.build(vertexShaderSrc, blurShaderSrc))             return false;
        if (!compositeProg.build(vertexShaderSrc, compositeShaderSrc))   return false;

        createFBOs(w, h);
        std::cerr << "Bloom pipeline initialized.\n";
        return true;
    }

    void resize(int w, int h) {
        sceneFBO.resize(w, h, true);
        thresholdFBO.resize(w / 2, h / 2, true);
        for (int i = 0; i < NUM_MIPS; ++i) {
            int mw = std::max(1, w >> (i + 1));
            int mh = std::max(1, h >> (i + 1));
            mipFBOs[i].resize(mw, mh, true);
            blurTempFBO[i].resize(mw, mh, true);
        }
    }

    // Execute the full bloom post-process (passes 2-4).
    // Assumes the scene has already been rendered into sceneFBO and the
    // given quad VAO is currently bound.
    void execute(const GLVertexArray& quad, const cfg::BloomConfig& bc,
                 bool cinematic = false, float time = 0.0f) {
        // Save GL state that we're about to clobber
        GLint prevProgram = 0, prevFBO = 0, prevActiveTexture = 0;
        GLint prevViewport[4] = {};
        GLint prevTex0 = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex0);
        /*--------- Pass 2: luminosity threshold ---------*/
        thresholdFBO.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        thresholdProg.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneFBO.colorTex);
        thresholdProg.set1i("sceneTex", 0);
        thresholdProg.set1f("threshold", bc.threshold);
        thresholdProg.set1f("softKnee", bc.softKnee);
        quad.drawQuad();

        /*--------- Pass 3: progressive downsample + separable blur ---------*/
        blurProg.use();
        GLuint srcTex = thresholdFBO.colorTex;
        for (int mip = 0; mip < NUM_MIPS; ++mip) {
            int mw = mipFBOs[mip].width;
            int mh = mipFBOs[mip].height;

            // Copy / downsample
            mipFBOs[mip].bind();
            glClear(GL_COLOR_BUFFER_BIT);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, srcTex);
            blurProg.set1i("inputTex", 0);
            blurProg.set2f("direction", 0.0f, 0.0f);
            blurProg.set2f("resolution", float(mw), float(mh));
            quad.drawQuad();

            // Horizontal blur
            blurTempFBO[mip].bind();
            glClear(GL_COLOR_BUFFER_BIT);
            glBindTexture(GL_TEXTURE_2D, mipFBOs[mip].colorTex);
            blurProg.set2f("direction", 1.0f, 0.0f);
            blurProg.set2f("resolution", float(mw), float(mh));
            quad.drawQuad();

            // Vertical blur
            mipFBOs[mip].bind();
            glClear(GL_COLOR_BUFFER_BIT);
            glBindTexture(GL_TEXTURE_2D, blurTempFBO[mip].colorTex);
            blurProg.set2f("direction", 0.0f, 1.0f);
            quad.drawQuad();

            // Additional blur passes for larger mips (capped at 2 for performance)
            int extraPasses = std::min(mip + 1, 2);
            for (int extra = 0; extra < extraPasses; ++extra) {
                blurTempFBO[mip].bind();
                glBindTexture(GL_TEXTURE_2D, mipFBOs[mip].colorTex);
                blurProg.set2f("direction", 1.0f, 0.0f);
                quad.drawQuad();

                mipFBOs[mip].bind();
                glBindTexture(GL_TEXTURE_2D, blurTempFBO[mip].colorTex);
                blurProg.set2f("direction", 0.0f, 1.0f);
                quad.drawQuad();
            }

            srcTex = mipFBOs[mip].colorTex;
        }

        /*--------- Pass 4: composite bloom onto scene ---------*/
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, sceneFBO.width, sceneFBO.height);
        glClear(GL_COLOR_BUFFER_BIT);
        compositeProg.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneFBO.colorTex);
        compositeProg.set1i("sceneTex", 0);

        for (int i = 0; i < NUM_MIPS; ++i) {
            glActiveTexture(GL_TEXTURE1 + i);
            glBindTexture(GL_TEXTURE_2D, mipFBOs[i].colorTex);
        }
        compositeProg.set1i("bloom1Tex", 1);
        compositeProg.set1i("bloom2Tex", 2);
        compositeProg.set1i("bloom3Tex", 3);
        compositeProg.set1i("bloom4Tex", 4);

        compositeProg.set1f("bloomIntensity", bc.intensity);
        compositeProg.set1f("bloom1Weight", bc.mipWeights[0]);
        compositeProg.set1f("bloom2Weight", bc.mipWeights[1]);
        compositeProg.set1f("bloom3Weight", bc.mipWeights[2]);
        compositeProg.set1f("bloom4Weight", bc.mipWeights[3]);
        compositeProg.set1f("exposure", bc.exposure);

        // Cinematic post-FX uniforms
        compositeProg.set1i("cinematicMode", cinematic ? 1 : 0);
        compositeProg.set1f("uTime", time);

        quad.drawQuad();

        // Restore prior GL state
        glUseProgram(prevProgram);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, prevTex0);
        glActiveTexture(prevActiveTexture);
    }

private:
    void createFBOs(int w, int h) {
        sceneFBO.create(w, h, true);
        thresholdFBO.create(w / 2, h / 2, true);
        for (int i = 0; i < NUM_MIPS; ++i) {
            int mw = std::max(1, w >> (i + 1));
            int mh = std::max(1, h >> (i + 1));
            mipFBOs[i].create(mw, mh, true);
            blurTempFBO[i].create(mw, mh, true);
        }
    }
};
