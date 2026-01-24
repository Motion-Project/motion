import { useState, useCallback, useEffect } from 'react'
import { useQueryClient } from '@tanstack/react-query'
import { useCameras, usePictures, useMovies, useMediaFolders, useDeletePicture, useDeleteMovie, useDeleteFolderFiles, queryKeys } from '@/api/queries'
import { useToast } from '@/components/Toast'
import { Pagination } from '@/components/Pagination'
import { getSessionToken } from '@/api/session'
import { useAuthContext } from '@/contexts/AuthContext'
import type { MediaItem, FolderFileItem } from '@/api/types'

type MediaType = 'pictures' | 'movies'

/**
 * Append session token to media URL for authentication
 * Required because img/video tags can't send custom headers
 */
function getAuthenticatedUrl(url: string): string {
  const token = getSessionToken()
  if (!token) return url
  const separator = url.includes('?') ? '&' : '?'
  return `${url}${separator}token=${encodeURIComponent(token)}`
}
type ViewMode = 'all' | 'folders'

const PAGE_SIZE = 100

// Parse date in YYYYMMDD format with optional time in HH:MM:SS format
function parseDateAndTime(dateStr: string, timeStr?: string): Date | null {
  // dateStr is YYYYMMDD format (e.g., "20250115")
  if (!dateStr || dateStr.length !== 8) return null

  const year = parseInt(dateStr.substring(0, 4), 10)
  const month = parseInt(dateStr.substring(4, 6), 10) - 1 // JS months are 0-indexed
  const day = parseInt(dateStr.substring(6, 8), 10)

  if (isNaN(year) || isNaN(month) || isNaN(day)) return null

  let hours = 0, minutes = 0, seconds = 0
  if (timeStr) {
    const timeParts = timeStr.split(':')
    if (timeParts.length >= 2) {
      hours = parseInt(timeParts[0], 10) || 0
      minutes = parseInt(timeParts[1], 10) || 0
      seconds = parseInt(timeParts[2], 10) || 0
    }
  }

  const date = new Date(year, month, day, hours, minutes, seconds)
  return isNaN(date.getTime()) ? null : date
}

export function Media() {
  const { addToast } = useToast()
  const { role } = useAuthContext()
  const queryClient = useQueryClient()
  const isAdmin = role === 'admin'
  const [selectedCamera, setSelectedCamera] = useState(1)
  const [mediaType, setMediaType] = useState<MediaType>('pictures')
  const [viewMode, setViewMode] = useState<ViewMode>('all')
  const [currentFolderPath, setCurrentFolderPath] = useState('')
  const [page, setPage] = useState(0)
  const [selectedItem, setSelectedItem] = useState<MediaItem | FolderFileItem | null>(null)
  const [deleteConfirm, setDeleteConfirm] = useState<MediaItem | FolderFileItem | null>(null)
  const [deleteAllConfirm, setDeleteAllConfirm] = useState<{ path: string; fileCount: number } | null>(null)

  const offset = page * PAGE_SIZE

  // Reset page when filters change
  useEffect(() => {
    setPage(0)
  }, [selectedCamera, mediaType, currentFolderPath])

  // Reset folder path when switching view modes
  useEffect(() => {
    if (viewMode === 'all') {
      setCurrentFolderPath('')
    }
  }, [viewMode])

  // Force refetch when switching views or navigating folders to ensure fresh data with thumbnails
  useEffect(() => {
    if (viewMode === 'all') {
      queryClient.invalidateQueries({ queryKey: queryKeys.movies(selectedCamera) })
      queryClient.invalidateQueries({ queryKey: queryKeys.pictures(selectedCamera) })
    }
  }, [viewMode, selectedCamera, queryClient])

  // Invalidate folder queries when folder path changes
  useEffect(() => {
    if (viewMode === 'folders') {
      queryClient.invalidateQueries({
        predicate: (query) =>
          Array.isArray(query.queryKey) &&
          query.queryKey[0] === 'media-folders' &&
          query.queryKey[1] === selectedCamera
      })
    }
  }, [viewMode, currentFolderPath, selectedCamera, queryClient])

  const { data: cameras } = useCameras()

  // Fetch paginated pictures/movies for "All" view
  const { data: picturesData, isLoading: picturesLoading } = usePictures(
    selectedCamera,
    offset,
    PAGE_SIZE,
    null, // no date filter in folder mode
    { enabled: viewMode === 'all' && mediaType === 'pictures' }
  )
  const { data: moviesData, isLoading: moviesLoading } = useMovies(
    selectedCamera,
    offset,
    PAGE_SIZE,
    null,
    { enabled: viewMode === 'all' && mediaType === 'movies' }
  )

  // Fetch folder contents for "Folders" view
  const { data: foldersData, isLoading: foldersLoading } = useMediaFolders(
    selectedCamera,
    currentFolderPath,
    offset,
    PAGE_SIZE,
    { enabled: viewMode === 'folders' }
  )

  const deletePictureMutation = useDeletePicture()
  const deleteMovieMutation = useDeleteMovie()
  const deleteFolderFilesMutation = useDeleteFolderFiles()

  const isLoading = viewMode === 'folders'
    ? foldersLoading
    : (mediaType === 'pictures' ? picturesLoading : moviesLoading)

  // Get current media data based on view mode
  const items: (MediaItem | FolderFileItem)[] = viewMode === 'folders'
    ? (foldersData?.files ?? [])
    : (mediaType === 'pictures'
        ? (picturesData?.pictures ?? [])
        : (moviesData?.movies ?? []))

  const totalCount = viewMode === 'folders'
    ? (foldersData?.total_files ?? 0)
    : (mediaType === 'pictures'
        ? (picturesData?.total_count ?? 0)
        : (moviesData?.total_count ?? 0))

  const formatSize = (bytes: number) => {
    if (bytes < 1024) return `${bytes} B`
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
    if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
    return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`
  }

  const formatDate = (dateStr: string, timeStr?: string) => {
    const date = parseDateAndTime(dateStr, timeStr)
    if (!date) return 'Unknown date'
    return date.toLocaleDateString() + ' ' + date.toLocaleTimeString()
  }

  const handleDeleteClick = useCallback((item: MediaItem | FolderFileItem, e: React.MouseEvent) => {
    e.stopPropagation()
    setDeleteConfirm(item)
  }, [])

  const handleDeleteConfirm = useCallback(async () => {
    if (!deleteConfirm) return

    try {
      // Determine if it's a picture or movie
      const itemType = 'type' in deleteConfirm ? deleteConfirm.type : mediaType
      if (itemType === 'picture' || itemType === 'pictures') {
        await deletePictureMutation.mutateAsync({
          camId: selectedCamera,
          pictureId: deleteConfirm.id,
        })
      } else {
        await deleteMovieMutation.mutateAsync({
          camId: selectedCamera,
          movieId: deleteConfirm.id,
        })
      }
      addToast(`${itemType === 'picture' || itemType === 'pictures' ? 'Picture' : 'Movie'} deleted`, 'success')
      setDeleteConfirm(null)
      if (selectedItem?.id === deleteConfirm.id) {
        setSelectedItem(null)
      }
    } catch {
      addToast('Failed to delete file', 'error')
    }
  }, [deleteConfirm, mediaType, selectedCamera, deletePictureMutation, deleteMovieMutation, addToast, selectedItem])

  const handleDeleteCancel = useCallback(() => {
    setDeleteConfirm(null)
  }, [])

  const handleDeleteAllClick = useCallback((path: string, fileCount: number) => {
    setDeleteAllConfirm({ path, fileCount })
  }, [])

  const handleDeleteAllConfirm = useCallback(async () => {
    if (!deleteAllConfirm) return

    try {
      const result = await deleteFolderFilesMutation.mutateAsync({
        camId: selectedCamera,
        path: deleteAllConfirm.path,
      })
      addToast(
        `Deleted ${result.deleted.movies} movies, ${result.deleted.pictures} pictures`,
        'success'
      )
      setDeleteAllConfirm(null)
    } catch {
      addToast('Failed to delete folder contents', 'error')
    }
  }, [deleteAllConfirm, selectedCamera, deleteFolderFilesMutation, addToast])

  const handleDeleteAllCancel = useCallback(() => {
    setDeleteAllConfirm(null)
  }, [])

  const navigateToFolder = useCallback((path: string) => {
    setCurrentFolderPath(path)
    setPage(0)
  }, [])

  const navigateUp = useCallback(() => {
    if (foldersData?.parent !== null) {
      setCurrentFolderPath(foldersData?.parent ?? '')
      setPage(0)
    }
  }, [foldersData])

  const isDeleting = deletePictureMutation.isPending || deleteMovieMutation.isPending
  const isDeletingAll = deleteFolderFilesMutation.isPending

  // Build breadcrumb navigation
  const breadcrumbs = currentFolderPath ? currentFolderPath.split('/') : []

  return (
    <div className="p-6">
      <div className="flex items-center justify-between mb-6">
        <h2 className="text-3xl font-bold">Media</h2>
      </div>

      {/* Camera and Type Selector */}
      <div className="flex flex-wrap gap-4 mb-6">
        <div>
          <label htmlFor="camera-select" className="block text-sm font-medium mb-2">Camera</label>
          <select
            id="camera-select"
            value={selectedCamera}
            onChange={(e) => setSelectedCamera(parseInt(e.target.value))}
            className="px-3 py-2 bg-surface border border-surface-elevated rounded-lg"
          >
            {cameras?.map((cam) => (
              <option key={cam.id} value={cam.id}>
                {cam.name}
              </option>
            ))}
          </select>
        </div>

        <div>
          <label className="block text-sm font-medium mb-2">Type</label>
          <div className="flex gap-2">
            <button
              onClick={() => setMediaType('pictures')}
              className={`px-4 py-2 rounded-lg transition-colors ${
                mediaType === 'pictures'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
              disabled={viewMode === 'folders'}
            >
              Pictures
            </button>
            <button
              onClick={() => setMediaType('movies')}
              className={`px-4 py-2 rounded-lg transition-colors ${
                mediaType === 'movies'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
              disabled={viewMode === 'folders'}
            >
              Movies
            </button>
          </div>
        </div>

        <div>
          <label className="block text-sm font-medium mb-2">View</label>
          <div className="flex gap-2">
            <button
              onClick={() => setViewMode('all')}
              className={`px-3 py-2 rounded-lg transition-colors text-sm ${
                viewMode === 'all'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
            >
              All
            </button>
            <button
              onClick={() => setViewMode('folders')}
              className={`px-3 py-2 rounded-lg transition-colors text-sm ${
                viewMode === 'folders'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
            >
              Folders
            </button>
          </div>
        </div>
      </div>

      {/* Folder Browser Header */}
      {viewMode === 'folders' && (
        <div className="mb-6 p-4 bg-surface-elevated rounded-lg">
          <div className="flex items-center gap-2 mb-3">
            <svg className="w-5 h-5 text-gray-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M3 7v10a2 2 0 002 2h14a2 2 0 002-2V9a2 2 0 00-2-2h-6l-2-2H5a2 2 0 00-2 2z" />
            </svg>
            <span className="text-sm font-medium text-gray-300">Browse Folders</span>
          </div>

          {/* Breadcrumb Navigation */}
          <div className="flex items-center gap-1 text-sm flex-wrap">
            <button
              onClick={() => navigateToFolder('')}
              className="hover:text-primary px-1"
            >
              Root
            </button>
            {breadcrumbs.map((crumb, index) => {
              const path = breadcrumbs.slice(0, index + 1).join('/')
              return (
                <span key={index} className="flex items-center gap-1">
                  <span className="text-gray-500">/</span>
                  <button
                    onClick={() => navigateToFolder(path)}
                    className="hover:text-primary px-1"
                  >
                    {crumb}
                  </button>
                </span>
              )
            })}
          </div>

          {/* Parent folder navigation */}
          {foldersData?.parent !== null && (
            <button
              onClick={navigateUp}
              className="mt-2 flex items-center gap-2 text-sm text-gray-400 hover:text-white"
            >
              <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 19l-7-7 7-7" />
              </svg>
              Up to parent folder
            </button>
          )}
        </div>
      )}

      {/* Folder List (when in folders view) */}
      {viewMode === 'folders' && foldersData && foldersData.folders.length > 0 && (
        <div className="mb-6 grid gap-2 md:grid-cols-2 lg:grid-cols-4">
          {foldersData.folders.map((folder) => (
            <div
              key={folder.path}
              className="bg-surface-elevated rounded-lg p-4 hover:ring-2 hover:ring-primary cursor-pointer transition-all group"
            >
              <button
                onClick={() => navigateToFolder(folder.path)}
                className="w-full text-left"
              >
                <div className="flex items-start gap-3">
                  <svg className="w-8 h-8 text-yellow-500 flex-shrink-0" fill="currentColor" viewBox="0 0 24 24">
                    <path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/>
                  </svg>
                  <div className="flex-1 min-w-0">
                    <p className="font-medium truncate">{folder.name}</p>
                    <p className="text-xs text-gray-400">
                      {folder.file_count} files - {formatSize(folder.total_size)}
                    </p>
                  </div>
                </div>
              </button>
              {/* Delete All button - admin only */}
              {isAdmin && folder.file_count > 0 && (
                <button
                  onClick={(e) => {
                    e.stopPropagation()
                    handleDeleteAllClick(folder.path, folder.file_count)
                  }}
                  className="mt-2 w-full px-2 py-1 text-xs bg-red-600/20 text-red-400 hover:bg-red-600/40 rounded opacity-0 group-hover:opacity-100 transition-opacity"
                  title={`Delete all ${folder.file_count} media files in this folder`}
                >
                  Delete All Media
                </button>
              )}
            </div>
          ))}
        </div>
      )}

      {/* Delete All for current folder (when viewing a folder with files) */}
      {viewMode === 'folders' && isAdmin && currentFolderPath && foldersData && foldersData.total_files > 0 && (
        <div className="mb-4 flex justify-end">
          <button
            onClick={() => handleDeleteAllClick(currentFolderPath, foldersData.total_files)}
            className="px-3 py-1.5 text-sm bg-red-600/20 text-red-400 hover:bg-red-600/40 rounded-lg flex items-center gap-2"
          >
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
            </svg>
            Delete All Media in This Folder ({foldersData.total_files})
          </button>
        </div>
      )}

      {/* Pagination Controls - Top */}
      {!isLoading && totalCount > 0 && (
        <Pagination
          offset={offset}
          limit={PAGE_SIZE}
          total={totalCount}
          onPageChange={(newOffset) => setPage(newOffset / PAGE_SIZE)}
          context={viewMode === 'folders' && currentFolderPath ? `in ${currentFolderPath}` : undefined}
        />
      )}

      {/* Gallery Grid */}
      {isLoading ? (
        <div className="grid gap-4 md:grid-cols-3 lg:grid-cols-4">
          {[1, 2, 3, 4, 5, 6, 7, 8].map((i) => (
            <div key={i} className="bg-surface-elevated rounded-lg animate-pulse">
              <div className="aspect-video bg-surface rounded-t-lg"></div>
              <div className="p-3">
                <div className="h-4 bg-surface rounded w-3/4 mb-2"></div>
                <div className="h-3 bg-surface rounded w-1/2"></div>
              </div>
            </div>
          ))}
        </div>
      ) : items.length === 0 ? (
        <div className="bg-surface-elevated rounded-lg p-8 text-center">
          <p className="text-gray-400">
            {viewMode === 'folders'
              ? 'No media files in this folder'
              : `No ${mediaType} found`}
          </p>
          <p className="text-sm text-gray-500 mt-2">
            {viewMode === 'folders'
              ? 'Navigate to a folder with media files'
              : (mediaType === 'pictures'
                  ? 'Motion detection snapshots will appear here'
                  : 'Recorded videos will appear here')}
          </p>
        </div>
      ) : (
        <div className="grid gap-4 md:grid-cols-3 lg:grid-cols-4">
          {items.map((item) => {
            const itemType = 'type' in item ? item.type : (mediaType === 'pictures' ? 'picture' : 'movie')
            const thumbnail = item.thumbnail || undefined
            return (
              <button
                key={`${viewMode}-${currentFolderPath}-${item.id}`}
                className="bg-surface-elevated rounded-lg overflow-hidden cursor-pointer hover:ring-2 hover:ring-primary focus:ring-2 focus:ring-primary focus:outline-none transition-all group relative text-left w-full"
                onClick={() => setSelectedItem(item)}
                aria-label={`View ${item.filename}`}
              >
                {/* Delete button - appears on hover */}
                <button
                  onClick={(e) => handleDeleteClick(item, e)}
                  className="absolute top-2 right-2 z-10 p-1.5 bg-red-600/80 hover:bg-red-600 rounded opacity-0 group-hover:opacity-100 group-focus-within:opacity-100 transition-opacity"
                  title="Delete"
                  aria-label={`Delete ${item.filename}`}
                >
                  <svg className="w-4 h-4 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                  </svg>
                </button>
                <div className="aspect-video bg-surface flex items-center justify-center relative overflow-hidden">
                  {itemType === 'picture' ? (
                    <img
                      src={getAuthenticatedUrl(item.path)}
                      alt={item.filename}
                      className="w-full h-full object-cover"
                      loading="lazy"
                    />
                  ) : thumbnail ? (
                    <img
                      src={getAuthenticatedUrl(thumbnail)}
                      alt={item.filename}
                      className="w-full h-full object-cover"
                      loading="lazy"
                      onError={(e) => {
                        e.currentTarget.style.display = 'none';
                        const parent = e.currentTarget.parentElement;
                        if (parent) {
                          const fallback = parent.querySelector('.fallback-icon');
                          if (fallback) {
                            (fallback as HTMLElement).style.display = 'flex';
                          }
                        }
                      }}
                    />
                  ) : null}
                  <div className={`fallback-icon text-gray-400 absolute inset-0 flex items-center justify-center ${thumbnail ? 'hidden' : ''}`}>
                    <svg className="w-16 h-16" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                    </svg>
                  </div>
                </div>
                <div className="p-3">
                  <p className="text-sm font-medium truncate">{item.filename}</p>
                  <div className="flex justify-between text-xs text-gray-400 mt-1">
                    <span>{formatSize(item.size)}</span>
                    <span>{item.date ? formatDate(item.date, item.time) : ''}</span>
                  </div>
                </div>
              </button>
            )
          })}
        </div>
      )}

      {/* Pagination Controls - Bottom */}
      {!isLoading && totalCount > 0 && (
        <Pagination
          offset={offset}
          limit={PAGE_SIZE}
          total={totalCount}
          onPageChange={(newOffset) => setPage(newOffset / PAGE_SIZE)}
          context={viewMode === 'folders' && currentFolderPath ? `in ${currentFolderPath}` : undefined}
        />
      )}

      {/* Media Viewer Modal */}
      {selectedItem && (
        <div
          className="fixed inset-0 bg-black/90 z-50 flex items-center justify-center p-4"
          onClick={() => setSelectedItem(null)}
        >
          <div
            className="max-w-6xl w-full bg-surface-elevated rounded-lg overflow-hidden"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="p-4 border-b border-surface flex justify-between items-center">
              <div>
                <h3 className="font-medium">{selectedItem.filename}</h3>
                <p className="text-sm text-gray-400">
                  {formatSize(selectedItem.size)} {selectedItem.date && `- ${formatDate(selectedItem.date, selectedItem.time)}`}
                </p>
              </div>
              <div className="flex gap-2">
                <a
                  href={getAuthenticatedUrl(selectedItem.path)}
                  download={selectedItem.filename}
                  className="px-3 py-1 bg-primary hover:bg-primary-hover rounded text-sm"
                  onClick={(e) => e.stopPropagation()}
                >
                  Download
                </a>
                <button
                  onClick={(e) => {
                    e.stopPropagation()
                    setDeleteConfirm(selectedItem)
                  }}
                  className="px-3 py-1 bg-red-600 hover:bg-red-700 rounded text-sm"
                >
                  Delete
                </button>
                <button
                  onClick={() => setSelectedItem(null)}
                  className="px-3 py-1 bg-surface hover:bg-surface-elevated rounded text-sm"
                >
                  Close
                </button>
              </div>
            </div>
            <div className="p-4">
              {('type' in selectedItem ? selectedItem.type : mediaType) === 'picture' ? (
                <img
                  src={getAuthenticatedUrl(selectedItem.path)}
                  alt={selectedItem.filename}
                  className="w-full h-auto max-h-[70vh] object-contain"
                />
              ) : (
                <video
                  src={getAuthenticatedUrl(selectedItem.path)}
                  controls
                  className="w-full h-auto max-h-[70vh]"
                  autoPlay
                >
                  Your browser does not support video playback.
                </video>
              )}
            </div>
          </div>
        </div>
      )}

      {/* Delete Confirmation Modal */}
      {deleteConfirm && (
        <div
          className="fixed inset-0 bg-black/80 z-[60] flex items-center justify-center p-4"
          onClick={handleDeleteCancel}
        >
          <div
            className="w-full max-w-md bg-surface-elevated rounded-lg"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="p-6">
              <h3 className="text-xl font-bold mb-2">Delete File?</h3>
              <p className="text-gray-400 mb-4">
                Are you sure you want to delete <span className="font-medium text-white">{deleteConfirm.filename}</span>?
                This action cannot be undone.
              </p>
              <div className="flex gap-3 justify-end">
                <button
                  onClick={handleDeleteCancel}
                  disabled={isDeleting}
                  className="px-4 py-2 bg-surface hover:bg-surface-elevated rounded-lg disabled:opacity-50"
                >
                  Cancel
                </button>
                <button
                  onClick={handleDeleteConfirm}
                  disabled={isDeleting}
                  className="px-4 py-2 bg-red-600 hover:bg-red-700 rounded-lg disabled:opacity-50"
                >
                  {isDeleting ? 'Deleting...' : 'Delete'}
                </button>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Delete All Confirmation Modal */}
      {deleteAllConfirm && (
        <div
          className="fixed inset-0 bg-black/80 z-[60] flex items-center justify-center p-4"
          onClick={handleDeleteAllCancel}
        >
          <div
            className="w-full max-w-md bg-surface-elevated rounded-lg"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="p-6">
              <h3 className="text-xl font-bold mb-2 text-red-400">Delete All Media Files?</h3>
              <p className="text-gray-400 mb-4">
                Are you sure you want to delete <span className="font-medium text-white">all {deleteAllConfirm.fileCount} media files</span> in folder <span className="font-mono text-primary">{deleteAllConfirm.path || 'root'}</span>?
              </p>
              <p className="text-sm text-yellow-500 mb-4">
                This will delete movies, pictures, and their thumbnails. Subfolders will NOT be deleted. This action cannot be undone.
              </p>
              <div className="flex gap-3 justify-end">
                <button
                  onClick={handleDeleteAllCancel}
                  disabled={isDeletingAll}
                  className="px-4 py-2 bg-surface hover:bg-surface-elevated rounded-lg disabled:opacity-50"
                >
                  Cancel
                </button>
                <button
                  onClick={handleDeleteAllConfirm}
                  disabled={isDeletingAll}
                  className="px-4 py-2 bg-red-600 hover:bg-red-700 rounded-lg disabled:opacity-50"
                >
                  {isDeletingAll ? 'Deleting...' : 'Delete All'}
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
