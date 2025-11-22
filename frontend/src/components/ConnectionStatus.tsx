interface ConnectionStatusProps {
  status: 'connected' | 'disconnected' | 'reconnecting';
}

/**
 * Connection status indicator component
 */
export function ConnectionStatus({ status }: ConnectionStatusProps) {
  const getStatusColor = () => {
    switch (status) {
      case 'connected':
        return '#10B981'; // Green
      case 'reconnecting':
        return '#F59E0B'; // Yellow
      case 'disconnected':
        return '#EF4444'; // Red
    }
  };

  const getStatusText = () => {
    switch (status) {
      case 'connected':
        return 'Connected';
      case 'reconnecting':
        return 'Reconnecting...';
      case 'disconnected':
        return 'Disconnected';
    }
  };

  return (
    <div className="flex items-center gap-2">
      <div
        className="w-3 h-3 rounded-full transition-colors"
        style={{ backgroundColor: getStatusColor() }}
      />
      <span className="text-sm" style={{ color: '#EEEEEE' }}>
        {getStatusText()}
      </span>
    </div>
  );
}
