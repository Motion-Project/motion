/**
 * Common validation utilities for Motion configuration
 *
 * Based on validation patterns from the original MotionEye implementation.
 * These patterns ensure user input is safe and valid before sending to backend.
 */

import { z } from 'zod'

// Regex patterns from original MotionEye (main.js:22-27)
// Note: These patterns are intentionally permissive for usability while
// still preventing obvious security issues like path traversal

/**
 * Device and camera names
 * Allows: letters, numbers, hyphens, underscores, plus signs, spaces
 */
export const deviceNamePattern = /^[A-Za-z0-9\-_+ ]*$/

/**
 * Filename patterns (for snapshot_filename, picture_filename, movie_filename)
 * Allows: letters, numbers, spaces, parentheses, forward slashes, dots, underscores, hyphens
 * Also allows:
 * - strftime format codes: %Y, %m, %d, %H, %M, %S, etc.
 * - Motion single-letter tokens: %v (camera), %q (event), %Q (labels), %t (device),
 *   %C (event string), %w (width), %h (height), %f (filename), %$ (device name)
 * - Motion curly-brace tokens: %{movienbr}, %{eventid}, %{host}, %{fps}, %{ver},
 *   %{sdevx}, %{sdevy}, %{sdevxy}, %{ratio}, %{action_user}, %{secdetect}
 * - Optional width specifier: %05{movienbr}, %2v, etc.
 */
export const filenamePattern = /^([A-Za-z0-9 ()/._-]|%\d*[CYmdHMSqvQtwhf$]|%\d*\{[a-z_]+\})*$/

/**
 * Directory paths (for target_dir, etc.)
 * Allows: letters, numbers, spaces, parentheses, forward slashes, dots, underscores, hyphens
 * Note: Does NOT allow .. to prevent directory traversal attacks
 */
export const dirnamePattern = /^[A-Za-z0-9 ()/._-]*$/

/**
 * Email addresses (for notification settings)
 * Permissive pattern that allows most valid email characters
 */
export const emailPattern = /^[A-Za-z0-9 _+.@^~<>,-]*$/

/**
 * Validate that a path does not contain directory traversal sequences
 */
export function hasPathTraversal(path: string): boolean {
  return path.includes('..') || path.includes('~/')
}

// ============================================================================
// Zod Schemas
// ============================================================================

/**
 * Device/camera name validation
 */
export const deviceNameSchema = z
  .string()
  .max(64, 'Name must be 64 characters or less')
  .regex(deviceNamePattern, 'Name can only contain letters, numbers, hyphens, underscores, plus signs, and spaces')

/**
 * Filename validation (for strftime-style filenames)
 */
export const filenameSchema = z
  .string()
  .max(255, 'Filename must be 255 characters or less')
  .regex(filenamePattern, 'Filename contains invalid characters. Use letters, numbers, underscores, hyphens, strftime codes (%Y, %m, %d), and Motion tokens (%{movienbr}, %v, etc.)')
  .refine((val) => !hasPathTraversal(val), {
    message: 'Filename cannot contain directory traversal sequences (.. or ~/)',
  })

/**
 * Directory path validation
 */
export const dirnameSchema = z
  .string()
  .max(4096, 'Path must be 4096 characters or less')
  .regex(dirnamePattern, 'Path contains invalid characters')
  .refine((val) => !hasPathTraversal(val), {
    message: 'Path cannot contain directory traversal sequences (.. or ~/)',
  })

/**
 * Email validation (permissive for notification settings)
 */
export const emailSchema = z
  .string()
  .max(255, 'Email must be 255 characters or less')
  .regex(emailPattern, 'Email contains invalid characters')

/**
 * Framerate validation (1-100 fps)
 */
export const framerateSchema = z.coerce
  .number()
  .int('Framerate must be a whole number')
  .min(1, 'Framerate must be at least 1')
  .max(100, 'Framerate cannot exceed 100')

/**
 * Quality validation (1-100 percent)
 */
export const qualitySchema = z.coerce
  .number()
  .int('Quality must be a whole number')
  .min(1, 'Quality must be at least 1%')
  .max(100, 'Quality cannot exceed 100%')

/**
 * Resolution width validation
 */
export const widthSchema = z.coerce
  .number()
  .int('Width must be a whole number')
  .min(160, 'Width must be at least 160 pixels')
  .max(4096, 'Width cannot exceed 4096 pixels')

/**
 * Resolution height validation
 */
export const heightSchema = z.coerce
  .number()
  .int('Height must be a whole number')
  .min(120, 'Height must be at least 120 pixels')
  .max(2160, 'Height cannot exceed 2160 pixels')

/**
 * Port number validation (1-65535)
 */
export const portSchema = z.coerce
  .number()
  .int('Port must be a whole number')
  .min(1, 'Port must be at least 1')
  .max(65535, 'Port cannot exceed 65535')

/**
 * Motion detection threshold validation
 */
export const thresholdSchema = z.coerce
  .number()
  .int('Threshold must be a whole number')
  .min(1, 'Threshold must be at least 1')
  .max(2147483647, 'Threshold is too large')

/**
 * Noise level validation (0-255)
 */
export const noiseLevelSchema = z.coerce
  .number()
  .int('Noise level must be a whole number')
  .min(0, 'Noise level must be at least 0')
  .max(255, 'Noise level cannot exceed 255')

/**
 * Log level validation (1-9)
 */
export const logLevelSchema = z.coerce
  .number()
  .int('Log level must be a whole number')
  .min(1, 'Log level must be at least 1')
  .max(9, 'Log level cannot exceed 9')

/**
 * Device ID validation (positive integer)
 */
export const deviceIdSchema = z.coerce
  .number()
  .int('Device ID must be a whole number')
  .min(1, 'Device ID must be at least 1')
  .max(999, 'Device ID cannot exceed 999')

/**
 * Generic positive integer validation
 */
export const positiveIntSchema = z.coerce
  .number()
  .int('Must be a whole number')
  .min(0, 'Must be 0 or greater')

/**
 * Boolean from string (handles "on"/"off", "true"/"false", "1"/"0")
 */
export const booleanStringSchema = z
  .string()
  .transform((val) => {
    const lower = val.toLowerCase()
    return lower === 'on' || lower === 'true' || lower === '1'
  })

// ============================================================================
// Validation Result Types
// ============================================================================

export interface ValidationResult {
  success: boolean
  error?: string
}

/**
 * Validate a config parameter value based on parameter name
 * Returns { success: true } if valid, or { success: false, error: "message" } if invalid
 */
export function validateConfigParam(
  paramName: string,
  value: string
): ValidationResult {
  // Map parameter names to their validators
  const validators: Record<string, z.ZodSchema> = {
    // Device/camera settings
    device_name: deviceNameSchema,
    camera_name: deviceNameSchema,
    device_id: deviceIdSchema,

    // File paths
    target_dir: dirnameSchema,
    snapshot_filename: filenameSchema,
    picture_filename: filenameSchema,
    movie_filename: filenameSchema,
    log_file: dirnameSchema,

    // Video settings
    framerate: framerateSchema,
    stream_maxrate: framerateSchema,
    width: widthSchema,
    height: heightSchema,

    // Quality settings
    stream_quality: qualitySchema,
    picture_quality: qualitySchema,

    // Ports
    stream_port: portSchema,
    webcontrol_port: portSchema,

    // Motion detection
    threshold: thresholdSchema,
    noise_level: noiseLevelSchema,
    minimum_motion_frames: positiveIntSchema,

    // System
    log_level: logLevelSchema,
  }

  const validator = validators[paramName]

  if (!validator) {
    // No specific validator - allow any value but still check for path traversal
    // on string values that might be paths
    if (typeof value === 'string' && hasPathTraversal(value)) {
      return {
        success: false,
        error: 'Value cannot contain directory traversal sequences (.. or ~/)',
      }
    }
    return { success: true }
  }

  const result = validator.safeParse(value)
  if (result.success) {
    return { success: true }
  }

  // Return the first error message (Zod v4 uses .issues instead of .errors)
  const firstIssue = result.error.issues[0]
  return {
    success: false,
    error: firstIssue?.message ?? 'Invalid value',
  }
}
