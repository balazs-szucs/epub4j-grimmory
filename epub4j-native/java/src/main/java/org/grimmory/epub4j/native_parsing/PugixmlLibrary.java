/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2025-2026 Grimmory contributors
 * Copyright (C) 2025-2026 Booklore contributors
 */
package org.grimmory.epub4j.native_parsing;

/**
 * Probe for the pugixml native library.
 *
 * <p>Delegates to {@link EpubNativeLibrary#isAvailable()} to trigger eager loading of the
 * shared library containing pugixml.</p>
 *
 * @author Grimmory
 */
public final class PugixmlLibrary {

    private PugixmlLibrary() {
        // Utility class
    }

    /**
     * Returns whether the pugixml native library is available.
     * Triggers eager loading of the shared library on first call.
     *
     * @return {@code true} iff pugixml is available and usable
     */
    public static boolean isAvailable() {
        return EpubNativeLibrary.isAvailable();
    }
}
