import { apiGet, apiPost, apiPatch, apiDelete } from './client';

export interface Profile {
  profile_id: number;
  camera_id: number;
  name: string;
  description?: string;
  is_default: boolean;
  created_at: number;
  updated_at: number;
  param_count: number;
}

export interface ProfileWithParams extends Profile {
  params: Record<string, string>;
}

export interface CreateProfileData {
  name: string;
  description?: string;
  camera_id: number;
  snapshot_current?: boolean;
  params?: Record<string, string>;
}

export interface ProfilesListResponse {
  status: string;
  profiles: Profile[];
}

export interface ProfileGetResponse {
  status: string;
  profile_id: number;
  camera_id: number;
  name: string;
  description?: string;
  is_default: boolean;
  created_at: number;
  updated_at: number;
  params: Record<string, string>;
}

export interface ProfileCreateResponse {
  status: string;
  profile_id: number;
}

export interface ProfileApplyResponse {
  status: string;
  requires_restart: string[];
}

export interface ProfileStatusResponse {
  status: string;
  message?: string;
}

/**
 * Configuration Profiles API
 * Manages camera configuration profiles for quick settings switching
 */
export const profilesApi = {
  /**
   * List all profiles for a camera
   * GET /0/api/profiles?camera_id=X
   */
  list: async (cameraId: number): Promise<Profile[]> => {
    const response = await apiGet<ProfilesListResponse>(
      `/0/api/profiles?camera_id=${cameraId}`
    );
    return response.profiles || [];
  },

  /**
   * Get specific profile with parameters
   * GET /0/api/profiles/{id}
   */
  get: async (profileId: number): Promise<ProfileWithParams> => {
    const response = await apiGet<ProfileGetResponse>(
      `/0/api/profiles/${profileId}`
    );
    return {
      profile_id: response.profile_id,
      camera_id: response.camera_id,
      name: response.name,
      description: response.description,
      is_default: response.is_default,
      created_at: response.created_at,
      updated_at: response.updated_at,
      param_count: Object.keys(response.params).length,
      params: response.params,
    };
  },

  /**
   * Create new profile
   * POST /0/api/profiles
   */
  create: async (data: CreateProfileData): Promise<number> => {
    const response = await apiPost<ProfileCreateResponse>('/0/api/profiles', data as unknown as Record<string, unknown>);
    return response.profile_id;
  },

  /**
   * Update profile parameters
   * PATCH /0/api/profiles/{id}
   */
  update: async (
    profileId: number,
    params: Record<string, string>
  ): Promise<void> => {
    await apiPatch<ProfileStatusResponse>(`/0/api/profiles/${profileId}`, params);
  },

  /**
   * Delete profile
   * DELETE /0/api/profiles/{id}
   */
  delete: async (profileId: number): Promise<void> => {
    await apiDelete<ProfileStatusResponse>(`/0/api/profiles/${profileId}`);
  },

  /**
   * Apply profile to camera configuration
   * POST /0/api/profiles/{id}/apply
   */
  apply: async (profileId: number): Promise<string[]> => {
    const response = await apiPost<ProfileApplyResponse>(
      `/0/api/profiles/${profileId}/apply`,
      {}
    );
    return response.requires_restart || [];
  },

  /**
   * Set profile as default for camera
   * POST /0/api/profiles/{id}/default
   */
  setDefault: async (profileId: number): Promise<void> => {
    await apiPost<ProfileStatusResponse>(
      `/0/api/profiles/${profileId}/default`,
      {}
    );
  },
};
