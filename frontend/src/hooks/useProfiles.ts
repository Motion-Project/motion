import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { profilesApi, type CreateProfileData, type Profile } from '../api/profiles';

/**
 * Query key factory for profiles
 */
export const profileKeys = {
  all: ['profiles'] as const,
  lists: () => [...profileKeys.all, 'list'] as const,
  list: (cameraId: number) => [...profileKeys.lists(), cameraId] as const,
  details: () => [...profileKeys.all, 'detail'] as const,
  detail: (profileId: number) => [...profileKeys.details(), profileId] as const,
};

/**
 * Hook to fetch all profiles for a camera
 */
export function useProfiles(cameraId: number) {
  return useQuery({
    queryKey: profileKeys.list(cameraId),
    queryFn: () => profilesApi.list(cameraId),
    staleTime: 1000 * 60 * 5, // 5 minutes
  });
}

/**
 * Hook to fetch a specific profile with parameters
 */
export function useProfile(profileId: number) {
  return useQuery({
    queryKey: profileKeys.detail(profileId),
    queryFn: () => profilesApi.get(profileId),
    enabled: profileId > 0,
    staleTime: 1000 * 60 * 5, // 5 minutes
  });
}

/**
 * Hook to create a new profile
 */
export function useCreateProfile() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (data: CreateProfileData) => profilesApi.create(data),
    onSuccess: (_, variables) => {
      // Invalidate the profiles list for the camera that was updated
      queryClient.invalidateQueries({
        queryKey: profileKeys.list(variables.camera_id)
      });
    },
  });
}

/**
 * Hook to update profile parameters
 */
export function useUpdateProfile() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: ({
      profileId,
      params
    }: {
      profileId: number;
      params: Record<string, string>
    }) => profilesApi.update(profileId, params),
    onSuccess: (_, variables) => {
      // Invalidate the specific profile detail
      queryClient.invalidateQueries({
        queryKey: profileKeys.detail(variables.profileId)
      });
      // Invalidate all profile lists to update param counts
      queryClient.invalidateQueries({
        queryKey: profileKeys.lists()
      });
    },
  });
}

/**
 * Hook to delete a profile
 */
export function useDeleteProfile() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (profileId: number) => profilesApi.delete(profileId),
    onMutate: async (profileId) => {
      // Cancel outgoing refetches
      await queryClient.cancelQueries({ queryKey: profileKeys.all });

      // Snapshot previous values
      const previousProfiles = queryClient.getQueriesData({ queryKey: profileKeys.lists() });

      // Optimistically update all profile lists to remove the deleted profile
      queryClient.setQueriesData<Profile[]>(
        { queryKey: profileKeys.lists() },
        (old) => old?.filter(p => p.profile_id !== profileId)
      );

      return { previousProfiles };
    },
    onError: (_, __, context) => {
      // Rollback on error
      if (context?.previousProfiles) {
        context.previousProfiles.forEach(([queryKey, data]) => {
          queryClient.setQueryData(queryKey, data);
        });
      }
    },
    onSettled: () => {
      // Always refetch after error or success
      queryClient.invalidateQueries({ queryKey: profileKeys.all });
    },
  });
}

/**
 * Hook to apply a profile to the camera configuration
 */
export function useApplyProfile() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (profileId: number) => profilesApi.apply(profileId),
    onSuccess: () => {
      // Invalidate config cache to trigger re-fetch of current settings
      // This ensures the UI reflects the newly applied profile settings
      queryClient.invalidateQueries({ queryKey: ['config'] });
    },
  });
}

/**
 * Hook to set a profile as the default for a camera
 */
export function useSetDefaultProfile() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: (profileId: number) => profilesApi.setDefault(profileId),
    onMutate: async (profileId) => {
      // Cancel outgoing refetches
      await queryClient.cancelQueries({ queryKey: profileKeys.all });

      // Snapshot previous values
      const previousProfiles = queryClient.getQueriesData({ queryKey: profileKeys.lists() });

      // Optimistically update all profiles - set the selected one as default
      queryClient.setQueriesData<Profile[]>(
        { queryKey: profileKeys.lists() },
        (old) => old?.map(p => ({
          ...p,
          is_default: p.profile_id === profileId
        }))
      );

      return { previousProfiles };
    },
    onError: (_, __, context) => {
      // Rollback on error
      if (context?.previousProfiles) {
        context.previousProfiles.forEach(([queryKey, data]) => {
          queryClient.setQueryData(queryKey, data);
        });
      }
    },
    onSettled: () => {
      // Always refetch after error or success
      queryClient.invalidateQueries({ queryKey: profileKeys.all });
    },
  });
}
