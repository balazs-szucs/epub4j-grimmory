/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2025-2026 Grimmory contributors
 * Copyright (C) 2025-2026 Booklore contributors
 */
package org.grimmory.epub4j.native_parsing;

/**
 * Public entry point for managing the epub4j-native library lifecycle.
 *
 * <p>This class is a thin, JVM-safe facade over the native-library loading logic
 * implemented in {@link PanamaConstants}. It exists so consumers have a clean,
 * stable API for:
 *
 * <ul>
 *   <li>Forcing the native library to be loaded at a controlled point ({@link #initialize()})</li>
 *   <li>Probing availability without triggering an exception ({@link #isAvailable()})</li>
 *   <li>Retrieving the load failure cause for diagnostics ({@link #loadError()})</li>
 * </ul>
 *
 * <h2>Concurrency model</h2>
 *
 * <p>Initialization is driven by the <b>Initialization-on-demand holder idiom</b>
 * (JLS §12.4.2): the JVM class-loader lock guarantees the native library is loaded
 * exactly once per JVM, under the class-loader initialization lock, before any
 * thread can observe {@link #isAvailable()} returning {@code true}. No explicit
 * synchronization is needed on the hot path.
 *
 * <p>This makes {@link #initialize()} and {@link #isAvailable()} safe to call
 * from any thread at any time, including concurrent {@code @BeforeAll} hooks
 * in a parallel JUnit test run.
 *
 * <h2>System properties</h2>
 *
 * <ul>
 *   <li><b>{@code epub4j.native.path}</b>, absolute filesystem path to the
 *       native library binary. When set, overrides classpath extraction and
 *       {@code System.loadLibrary} lookup. Also honoured as the environment
 *       variable {@code EPUB4J_NATIVE_PATH}. See {@link PanamaConstants}.</li>
 *   <li><b>{@code epub4j.native.maxCStringBytes}</b>, maximum length (in
 *       bytes) to scan when converting native C strings to Java strings.
 *       Default: 4 MB.</li>
 * </ul>
 *
 * <h2>Example</h2>
 *
 * <pre>{@code
 * if (EpubNativeLibrary.isAvailable()) {
 *     try (NativeArchive archive = NativeArchive.open(path)) {
 *         // ...
 *     }
 * } else {
 *     log.warn("epub4j-native unavailable: {}", EpubNativeLibrary.loadError());
 * }
 * }</pre>
 */
public final class EpubNativeLibrary {

    /** See {@link PanamaConstants} for the canonical documentation. */
    public static final String PROP_LIBRARY_PATH = "epub4j.native.path";

    private EpubNativeLibrary() {
        // Utility class, should not be instantiated.
    }

    /**
     * Ensures the epub4j-native shared library is loaded. Safe to call from any thread,
     * any number of times. Does nothing on subsequent calls after the first.
     *
     * @throws RuntimeException if the native library could not be loaded. The original
     *                          cause is available via {@link #loadError()}.
     */
    public static void initialize() {
        Holder.STATE.require();
    }

    /**
     * Returns whether the native library has been successfully loaded.
     *
     * <p>Triggers loading on first call; returns {@code false} (not throws) if the
     * library is not available. Safe to call from any thread, never throws.
     *
     * @return {@code true} iff the native library is loaded and usable in this JVM
     */
    public static boolean isAvailable() {
        return Holder.STATE.loaded;
    }

    /**
     * Returns the {@link Throwable} that caused the native library to fail loading,
     * or {@code null} if loading succeeded or has not yet been attempted. Useful for
     * logging without forcing a re-throw.
     */
    public static Throwable loadError() {
        return Holder.STATE.loadError;
    }

    private static final class Holder {
        static final State STATE = new State();
    }

    private static final class State {
        final boolean loaded;
        final Throwable loadError;

        State() {
            Throwable err = null;
            boolean ok = false;
            try {
                Class.forName(
                        "org.grimmory.epub4j.native_parsing.PanamaConstants",
                        true,
                        EpubNativeLibrary.class.getClassLoader());
                ok = true;
            } catch (Throwable t) {
                // Throwable, not Exception, covers UnsatisfiedLinkError,
                // ExceptionInInitializerError, NoClassDefFoundError, and their
                // kin. Unwrap the initializer error so callers see the real cause.
                err = (t instanceof ExceptionInInitializerError eie && eie.getCause() != null)
                        ? eie.getCause()
                        : t;
            }
            this.loaded = ok;
            this.loadError = err;
        }

        void require() {
            if (loaded) return;
            if (loadError instanceof RuntimeException re) throw re;
            throw new RuntimeException("Failed to initialize epub4j-native", loadError);
        }
    }
}
