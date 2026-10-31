import DevSignature from '../components/DevSignature';
import GeneratorPage from './GeneratorPage';

export default function DevPage() {
    return <GeneratorPage mode="development" SignatureComponent={DevSignature} />;
}
