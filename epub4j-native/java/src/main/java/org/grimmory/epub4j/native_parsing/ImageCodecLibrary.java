/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2025-2026 Grimmory contributors
 * Copyright (C) 2025-2026 Booklore contributors
 */
package org.grimmory.epub4j.native_parsing;

/**
 * Probe for the image processing native libraries (libjpeg-turbo, libpng, libwebp).
 *
 * <p>Delegates to {@link EpubNativeLibrary#isAvailable()} to trigger eager loading of the
 * shared library containing the image codecs.</p>
 *
 * @author Grimmory
 */
public final class ImageCodecLibrary {

    private ImageCodecLibrary() {
        // Utility class
    }

    /**
     * Returns whether the image processing native libraries are available.
     * Triggers eager loading of the shared library on first call.
     *
     * @return {@code true} iff the image codecs are available and usable
     */
    public static boolean isAvailable() {
        return EpubNativeLibrary.isAvailable();
    }
}
