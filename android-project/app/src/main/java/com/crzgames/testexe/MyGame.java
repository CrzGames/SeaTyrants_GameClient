package com.crzgames.testexe;

import org.libsdl.app.SDLActivity;

/**
 * A sample wrapper class that just calls SDLActivity
 */

public class MyGame extends SDLActivity {
    /**
     * Override this to load additional libraries
     */
    @Override
    protected String[] getLibraries() {
        return new String[] { 
            /**
             * Lib : SDL3, SDL3_image, SDL3_mixer, SDL3_ttf
             * 
             * SDL3 dois être chargé avant les autres libs SDL3_* pour que les symboles de
             * SDL3_image, SDL3_mixer et SDL3_ttf soient résolus correctement
             */
            "SDL3",
            "SDL3_image",
            "SDL3_mixer",
            "SDL3_ttf",
            /**
             * Lib : ffmpeg
             */
            "avcodec",
            "avdevice",
            "avfilter",
            "avformat",
            "avutil",
            "swresample",
            "swscale",
            /**
             * Lib : onnxruntime
             */
            "onnxruntime",
            /**
             * Lib : main
             *
             * C'est la lib qui contient le code de l'application, et elle doit être chargé
             * après toutes les autres libs pour que les symboles soient résolus correctement
             */
            "main"
        };
    }
}